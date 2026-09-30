# 17 — The memory model: a lock-free SPSC ring buffer

**Time:** 2 h · **Gringofts link:** the `std::atomic` flags and counters throughout
(`MpscDoubleBufferQueue.h`, loop classes), and the handful of explicit `memory_order` /
`compare_exchange` uses in `src/`. Read this exercise before you touch any of them.

## Why
You'll rarely *write* lock-free code, but you'll *read* it, and review changes to it.
Java's memory model (volatile = sequentially consistent, `happens-before` through locks) has a
C++ counterpart with more knobs:

| C++ | Roughly in Java | Use |
|---|---|---|
| `std::atomic<T>` default (`memory_order_seq_cst`) | `volatile` / `AtomicLong` | the default; use it unless a profiler says otherwise |
| `memory_order_release` on a store | writes before it are visible to whoever *acquires* this value | publishing data ("the slot is ready") |
| `memory_order_acquire` on a load | sees everything the releasing thread wrote before its release store | consuming published data |
| `memory_order_relaxed` | atomicity only, **no ordering** | statistics counters (ex 10) |
| plain `int`/`bool` shared between threads | a data race: **undefined behaviour** | never |

The pattern to recognise is **release/acquire publication**: the producer writes the data, *then*
release-stores an index. The consumer acquire-loads the index, *then* reads the data. Weaken
either side to `relaxed` and the consumer may read a slot before the producer's write lands.

## Tasks (`src/spsc.h`)
Implement `tryPush` / `tryPop` for a single-producer / single-consumer ring:
- `mHead`/`mTail` only ever increase; slot index is `counter & (N - 1)` (N is a power of two).
- Full when `tail - head == N`; empty when `head == tail`.
- Each side loads **its own** index relaxed (only it writes it) and the **other side's** with acquire,
  and publishes its own with release.

## Experiments (the important part)
1. Build the TSan variant (`-DJ2C_SANITIZER=thread -DJ2C_USE_SOLUTIONS=ON`) and run `ex17`: clean.
2. Replace every `memory_order_acquire`/`release` with `memory_order_relaxed` and run under TSan again:
   `WARNING: ThreadSanitizer: data race`, with the slot read in `tryPop` racing the slot write in
   `tryPush`. On x86 the plain build may still *pass*: x86 hardware orders more strictly than C++
   requires. On ARM servers (Graviton, Ampere) it can fail for real. **Tests passing on x86 prove nothing about ordering.**
3. Remove the `alignas(64)` and time the stress test in a Release build. That's false sharing.
4. Why can't this be used with two producers? Where would two producers race? (That's why
   Gringofts' multi-producer queue uses a mutex and swaps buffers instead.)

## Rules for code review
- Any `memory_order` weaker than the default needs a comment saying what it pairs with.
- Lock-free code must have a TSan-clean stress test.
- Prefer a mutex. Reach for lock-free only with a profile showing contention.
