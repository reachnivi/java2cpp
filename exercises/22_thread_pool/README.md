# 22 — A thread pool with futures

**Time:** 2.5 h · **Part 3.** Java hands you `ExecutorService`. The C++17 standard library has no pool,
only the building blocks: `std::thread`, `std::packaged_task`, `std::future`/`std::promise`.
Gringofts uses dedicated threads per loop (ex 10) rather than a pool, but you'll meet pools in libraries
and write one for background work (snapshots, compaction, metrics).

## The pieces
| Java | C++ |
|---|---|
| `Callable<R>` | any callable returning `R` |
| `FutureTask<R>` | `std::packaged_task<R()>`: wraps a callable and fills a shared state with its result **or its exception** |
| `Future<R>.get()` | `std::future<R>::get()`: blocks, returns the value or **rethrows** the task's exception |
| `CompletableFuture.complete(v)` | `std::promise<R>::set_value(v)` |
| `executor.shutdown(); awaitTermination()` | your `shutdown()`: stop accepting, drain, join |

## Tasks (`src/thread_pool.h`)
1. Constructor starts N workers running `workerLoop()`.
2. `workerLoop`: wait with a predicate; exit only when stopping **and** drained; run jobs **outside** the lock.
3. `submit(f, args...)`: `packaged_task` + `std::apply` over a tuple of the args (move-only args must work);
   return the future; throw after shutdown.
4. `shutdown()`: idempotent; drains, then joins. The destructor calls it.

## Rules the tests enforce, and why
- **Exceptions must not kill a worker.** `packaged_task` catches them and stores them in the future. Without it,
  an exception escaping a thread function is `std::terminate()` (crash lab 02).
- **Never hold the queue lock while running a task.** A task that calls `submit` would deadlock on the
  same mutex (crash lab 09), and one slow task would block every other worker.
- **Don't block a worker waiting on another task's future** in a small pool: if every worker waits, nobody runs
  the tasks being waited for (thread-starvation deadlock). The `TaskCanSubmitTask` test returns the inner future instead.

## Experiments
- In `TaskCanSubmitTask`, make the outer task call `inner.get()` with a 1-thread pool. Why does it hang?
- Replace the `shared_ptr<packaged_task>` with a plain capture of the packaged_task in `std::function`: read the error.
  Then swap `std::function` for your ex 20 `UniqueFunction` and it compiles.
- Add `std::future::wait_for` timeouts and work out how to *cancel* a queued task (hint: a shared atomic flag).
