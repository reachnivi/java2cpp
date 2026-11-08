# 19 — Build your own `shared_ptr` / `weak_ptr`

**Time:** 3 h · **Part 3.** Gringofts passes `std::shared_ptr<Command>` and `std::shared_ptr<Event>` between
threads everywhere. After this you'll know exactly what each copy costs and when the object dies.

## How it works
```
SharedPtr<T> ──┐                  ┌───────────────── control block ─────────────────┐
SharedPtr<T> ──┼──► T* object     │ strong = 2   (owners of the OBJECT)              │
WeakPtr<T>  ───┴──────────────────► weak   = 2   (owners of the BLOCK: 1 per WeakPtr  │
                                  │                + 1 shared by all strong owners)  │
                                  │ virtual destroyObject() / destroySelf()          │
                                  └──────────────────────────────────────────────────┘
```
- strong → 0: **destroy the object** (call the deleter / run `~T()`), then drop the strong owners' weak count.
- weak → 0: **free the control block**.
- `makeShared` puts the object **inside** the block: one allocation instead of two, better locality.
  The trade-off: while any `WeakPtr` exists, the object's *memory* stays allocated even after `~T()` ran.
- Virtual functions in the block are **type erasure**: `SharedPtr<T>` never knows the deleter's type.
- Counts are atomic (`fetch_add` / `fetch_sub`), so copying a `shared_ptr` across threads is safe. That's
  also why copying one costs more than you'd think (a locked bus operation). Pass `const SharedPtr&` when you don't need ownership.

## Tasks (`src/shared_ptr.h`)
`PointerControlBlock` and the class skeletons are given. Implement:
1. `releaseStrong`, `releaseWeak`, `tryAddStrong` (a CAS loop: `lock()` must never resurrect a dead object).
2. `InlineControlBlock`: placement new into `mStorage`, explicit destructor call.
3. `SharedPtr`: constructor (don't leak `p` if allocating the block throws), copy, move, assignment, destructor.
4. `makeShared` and `WeakPtr` (`lock`, `expired`, copy, destructor).

## Experiments
- Implement `tryAddStrong` as `fetch_add` followed by a check of the old value. Why is that a bug?
  (Think: thread A's last `releaseStrong` is destroying the object while B "revives" it.)
- Change `fetch_sub(1, acq_rel)` to `relaxed` and run `ex19` under TSan.
- In `makeShared`, remove the `static_cast` the comments warn about and read the compiler error.
  Overload resolution between templates and non-templates is a classic C++ surprise.
- Read libstdc++'s `_Sp_counted_base` in `/usr/include/c++/*/bits/shared_ptr_base.h` and compare.
