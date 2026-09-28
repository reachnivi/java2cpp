# 02 — RAII: destructors instead of `finally`

**Time:** 1 h · **Gringofts link:** `std::lock_guard<std::mutex>` / `std::unique_lock`
everywhere (e.g. `src/infra/mpscqueue/MpscDoubleBufferQueue.cpp`), RocksDB handles
in `src/infra/raft/storage/`, `std::thread` members joined in destructors.

## Why
C++ has no garbage collector and no `finally`. Instead, **a destructor runs
deterministically when an object goes out of scope** — including during
exception unwinding. "Resource Acquisition Is Initialization" (RAII) = acquire
in the constructor, release in the destructor.

```java
// Java                                 // C++
lock.lock();                            {
try { work(); }                           std::lock_guard<std::mutex> g(mMutex);
finally { lock.unlock(); }                work();
                                        }  // unlocked here, even on throw
```
`try-with-resources` is the closest Java concept, except in C++ *every* object
behaves that way, and you never have to remember to use it.

## Tasks (`src/raii.h`)
1. **`LiveCounter`** — increments a static counter `alive()` in its constructor,
   decrements in its destructor. Proves to you when destructors run.
2. **`ScopeGuard`** — takes a `std::function<void()>`, runs it in the destructor
   unless `dismiss()` was called. (Like Go's `defer`.) Make it non-copyable:
   `ScopeGuard(const ScopeGuard &) = delete;`
3. **`FileHandle`** — wraps a C `FILE *`.
   - ctor opens (`std::fopen`), throws `std::runtime_error` if it fails.
   - dtor closes if non-null.
   - **non-copyable** (two owners would double-close) but **movable**: the
     moved-from handle must hold `nullptr`. (Preview of ex 03.)
   - `write(const std::string&)`, `isOpen()`.

## Think about
- Why must destructors never throw? (Hint: what if one runs during unwinding
  from another exception → `std::terminate`.)
- Order: members are destroyed in *reverse* declaration order. Gringofts
  relies on this in classes that own both a thread and the queue it reads.
