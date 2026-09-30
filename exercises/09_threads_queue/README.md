# 09 — Threads, mutexes, condition variables: build an MPSC queue

**Time:** 2 h · **Gringofts link:** `src/infra/mpscqueue/MpscQueue.h` (interface),
`MpscDoubleBufferQueue.{h,cpp}` (implementation), `src/infra/util/BlockingQueue`
usage in `App.h` (`BlockingQueue<std::shared_ptr<Command>> mCommandQueue`).
Commands flow: gRPC threads (many producers) → queue → one `CommandProcessLoop` thread.

## Java → C++ concurrency map
| Java | C++17 |
|---|---|
| `new Thread(r).start()` | `std::thread t(fn);` — starts immediately |
| `t.join()` | `t.join()` — **you must join or detach before the `std::thread` is destroyed, else `std::terminate()`** |
| `synchronized` / `ReentrantLock` | `std::mutex` + `std::lock_guard<std::mutex>` / `std::unique_lock` (not reentrant!) |
| `wait()/notify()` / `Condition` | `std::condition_variable` (needs `unique_lock`) |
| `volatile` / `AtomicLong` | `std::atomic<T>` (C++ `volatile` is **not** for threading) |
| `ExecutorService` | none in std; Gringofts uses dedicated `std::thread`s per loop |
| `BlockingQueue` | write your own — this exercise |

A **data race** (two threads, one writing, no synchronization) is undefined
behaviour in C++, not just a stale read. Use ThreadSanitizer:
`cmake -B build-tsan -DJ2C_SANITIZER=thread && cmake --build build-tsan && ctest --test-dir build-tsan`.

## Tasks (`src/mpsc_queue.h`)
Implement `BlockingMpscQueue<T>` with the same contract as Gringofts' `MpscQueue<T>`:
- `enqueue(const T&)` — thread-safe; throws `QueueStoppedException` after shutdown.
- `T dequeue()` — blocks while empty. After `shutdown()`, keeps returning
  remaining items, then throws `QueueStoppedException` once drained.
- `size()`, `empty()`, `shutdown()` (wakes a blocked consumer).

Rules: always `wait` with a predicate (spurious wakeups exist):
```cpp
std::unique_lock<std::mutex> lock(mMutex);
mCondVar.wait(lock, [this] { return !mQueue.empty() || mShutdown; });
```

## Stretch
Read `MpscDoubleBufferQueue.cpp`: producers push to one list, the consumer
swaps lists under the lock, then drains its own list lock-free. Implement that
variant and compare throughput with a tiny benchmark.
