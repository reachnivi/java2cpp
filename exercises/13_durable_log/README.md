# 13 — Durable append-only log: POSIX I/O, fsync, torn writes

**Time:** 2.5 h · **Gringofts link:** `src/infra/raft/storage/Segment.cpp` (the Raft log's
on-disk segments, mmap'd), `src/infra/util/FileUtil.{h,cpp}`, the `fsync`/sync calls in
storage code, and RocksDB's `WriteOptions::sync`.

Raft can promise "committed" only if the log entry actually survives a power cut. In Java
you'd reach for `FileChannel.force(true)`. In C++ on Linux you use the POSIX file API
directly, and every byte of the durability story is your responsibility.

## What you need to know
- `open(2)` returns a file descriptor (an `int`) or `-1` and sets `errno`. Wrap it in an
  RAII class (ex 02) so it's always `close(2)`d. Report failures as
  `std::system_error(errno, std::generic_category(), "open " + path)`, the C++ way to carry errno.
- `write(2)` may write **fewer bytes than requested** and may fail with `EINTR`. Loop until done.
- `write` only reaches the page cache. The data survives a process crash but **not a power loss**,
  until `fsync(2)`/`fdatasync(2)` returns. After creating a new file you must also fsync the
  **directory** so the file's name is durable. (Gringofts' segments rely on the same rules.)
- A crash in the middle of an append leaves a **torn tail**: half a record at the end of the
  file. Recovery must detect it (length + checksum) and truncate it away (`ftruncate(2)`).
- A bad record **followed by valid data** isn't a torn write. It's corruption (disk, bug, or
  someone else writing the file). Failing loudly is the only safe answer: silently dropping
  committed entries would make replicas diverge.

## Frame format
```
[uint32 length][uint32 crc32(payload)][payload bytes]    (little-endian, host order is fine here)
```

## Tasks (`src/durable_log.h`)
`crc32()` is provided.
1. `Fd`: RAII, move-only wrapper around a file descriptor. `Fd(path, flags, mode)` throws
   `std::system_error` on failure. `get()`, destructor closes.
2. `SegmentWriter::open(path)`: `O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC`, mode `0644`.
3. `SegmentWriter::append(payload)`: write one frame with a full-write loop (handle short writes and `EINTR`).
4. `SegmentWriter::sync()`: `fdatasync`; throw `std::system_error` on failure.
5. `recover(path)`: read the whole file, parse frames, and:
   - stop at a frame that runs past EOF, or has a bad CRC **and is the last frame**. That's a torn
     tail: `truncate` the file to the last valid byte and report `truncatedTail = true`;
   - if a frame has a bad CRC but **more data follows it**, throw `std::runtime_error` with the offset.
   - A missing file recovers as empty.

## Experiments
- `strace -f -e trace=openat,write,fdatasync,ftruncate ./build/ex13` and watch what your code actually does.
- Remove the short-write loop and write a 64 MB payload to a pipe instead of a file (`/dev/stdout | cat > /dev/null`). What happens?
- Read `Segment.cpp` in Gringofts: where does it sync, and what does it do on recovery?
