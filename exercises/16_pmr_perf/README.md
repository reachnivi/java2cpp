# 16 — Allocation, `std::pmr`, and finding where the time goes

**Time:** 2 h · **Gringofts link:** `src/infra/util/MemoryPool.{h,cpp}`,
`TrackingMemoryResource.{h,cpp}`, `PMRContainerFactory.h` (Gringofts' own pmr setup, ~15 uses),
and the throughput numbers in Gringofts' README (8,000 tx/s per cluster, where allocation and copying matter).

## Why
In Java, allocation is a pointer bump in a thread-local buffer and the GC frees memory in bulk.
In C++, every `new` (every `std::string` longer than ~15 chars, every `vector` growth,
every `make_shared`) goes to a general-purpose `malloc`: a lock or thread cache, bookkeeping,
fragmentation. On a hot path handling thousands of commands a second, allocations are often
the biggest cost after I/O.

**Polymorphic memory resources** (`<memory_resource>`, C++17) let a container get its memory from
a resource you choose at run time:
- `std::pmr::monotonic_buffer_resource`: an arena. Allocation is a pointer bump, deallocation is a no-op,
  and everything is freed at once when the arena dies. Ideal for per-request or per-batch data.
- `std::pmr::unsynchronized_pool_resource` / `synchronized_pool_resource`: size-class pools.
- Your own: subclass `std::pmr::memory_resource` (for example, to track usage, as Gringofts does).

`std::pmr::vector<std::pmr::string>` passes its resource down to the strings inside it automatically
("uses-allocator construction"). `std::pmr::vector<std::string>` does **not**. One of the tests pins down that pitfall.

## Tasks (`src/pmr.h`)
1. `CountingResource`: count allocations, deallocations, outstanding bytes and peak bytes; fix `do_is_equal`.
2. `makeEventBatch(n, size, mr)`: all memory from `mr`, with exactly `n + 1` allocations.
3. Read the `Arena` tests: they show the monotonic arena's trade-off (fast; memory freed only when the arena dies).

## Measuring for real (on your machine, not in this container)
```bash
cmake -S . -B build-rel -DCMAKE_BUILD_TYPE=Release -DJ2C_USE_SOLUTIONS=ON && cmake --build build-rel --target ex16
./build-rel/ex16 --gtest_filter='Bench.*'                      # rough timing printout
perf stat -e task-clock,page-faults ./build-rel/ex16 --gtest_filter='Bench.*'
perf record -g ./build-rel/ex16 --gtest_filter='Bench.*' && perf report   # where is time spent? malloc? memcpy?
# flame graph: https://github.com/brendangregg/FlameGraph (stackcollapse-perf.pl | flamegraph.pl)
```
Rules of thumb:
- **Measure before optimising**, in a Release build (`-O2`), on realistic data. Debug-build timings mean nothing.
- Look first for copies you didn't intend (`auto x = bigThing;`, passing by value, missing `std::move`,
  `std::function` captures), then allocations in loops (`reserve()`, reuse buffers), then lock contention
  (`perf lock`, or many threads stuck in `futex_wait` in `thread apply all bt`).
- For microbenchmarks use [Google Benchmark](https://github.com/google/benchmark). It handles warm-up,
  repetitions and `DoNotOptimize`, which hand-rolled timers get wrong.
- `heaptrack` shows *who* allocates most. Start there when memory or allocation cost is the question.

## Think about
- Why is `deallocate` a no-op in the monotonic arena, and when does that become a memory leak? (A long-lived arena.)
- What happens if a `pmr` container outlives its resource? (Crash lab 06, again: dangling.)
