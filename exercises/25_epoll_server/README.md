# 25 — A non-blocking TCP server with `epoll`

**Time:** 3–4 h · **Part 3.** gRPC hides all of this, but its completion-queue threads (ex 15) sit on
exactly this machinery. When a Gringofts node shows a stuck connection, a CPU spike in a network thread, or
`Too many open files`, this is the layer you'll be reasoning about. Java equivalent: NIO's `Selector`,
or Netty's event loop.

## The model
One thread, many connections, nothing ever blocks:
```
epoll_wait() ──► ready fds ──► listen fd : accept4() until EAGAIN
                          ├──► client fd : EPOLLIN  → read() until EAGAIN → frame lines → handler → queue reply → flush
                          │                EPOLLOUT → flush the rest of the reply
                          └──► eventfd   : stop() wants us out
```
Rules you'll get wrong at least once (each has a test):
1. **Reads are partial.** TCP is a byte stream, not a message stream. "hello\n" can arrive as "hel" + "lo\n", or
   glued to the next message. Buffer per connection and **frame** (here: newline-delimited).
2. **Writes are partial.** `send()` can accept fewer bytes than you gave it when the socket buffer is full. Keep the rest
   and ask for `EPOLLOUT`.
3. **Stop asking for `EPOLLOUT` once drained.** Otherwise, with level-triggered epoll, every `epoll_wait` returns
   immediately and the thread spins at 100% CPU.
4. **`read() == 0` means the peer closed.** Close your side and release everything for that fd.
5. **`MSG_NOSIGNAL`** (or ignoring `SIGPIPE`): writing to a closed socket otherwise kills the whole process (exit 141).
6. **Waking a blocked `epoll_wait`**: from another thread, write to an `eventfd` that epoll is watching. That's the same
   trick as a condition variable notify, for file descriptors.
7. **Every fd is a resource** (RAII again): leaked fds end in `EMFILE: Too many open files`. Check with `ls /proc/<pid>/fd | wc -l`.

## Tasks (`src/epoll_server.h`)
`start()` (socket/bind/listen/epoll/eventfd setup), `stop()`, `watch`, `rewatch` and `onWritable` are given. Implement
`loop`, `acceptAll`, `onReadable` (with framing), `flush`, `closeConn`.
The tests skip themselves if the machine has no loopback networking.

## Experiments
- Leave `EPOLLOUT` registered after a flush completes, then watch `top -H` while a client is connected but idle.
- Remove `MSG_NOSIGNAL`; write a client that sends a request and closes immediately. What's the server's exit status?
- `strace -f -e trace=epoll_wait,read,write,sendto,accept4 ./build/ex25 --gtest_filter='*Split*'`: watch the partial reads happen.
- Switch to **edge-triggered** (`EPOLLET`). Which loops must now drain until `EAGAIN`, and what hangs if they don't?
- One loop thread is the limit here. Real servers run N loops (one per core) with `SO_REUSEPORT`, or hand work to a pool (ex 22).
