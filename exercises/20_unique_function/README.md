# 20 — A move-only `std::function`: type erasure and small-buffer optimisation

**Time:** 3 h · **Part 3.** Gringofts stores callbacks everywhere (`std::function` in
`Signal.h`, completion handlers, loop hooks). Sooner or later you'll try to capture a
`std::unique_ptr` in one and hit a wall of "use of deleted function" errors. This exercise explains why, and fixes it.

## Two big ideas
**Type erasure.** `UniqueFunction<int(int)>` can hold *any* callable: a lambda (each has its own unique type),
a function pointer, a functor. It works by generating, per callable type `Fn`, a small table of
plain function pointers (`invoke`, `moveTo`, `destroy`) and storing a pointer to that table. It's
the same idea as a vtable, built by hand, so the object itself doesn't need a virtual base class.
(`std::function`, `std::any` and Abseil's `AnyInvocable` all work this way.)

**Small-buffer optimisation (SBO).** Most lambdas capture a pointer or two. Heap-allocating for each
one would be wasteful, so callables that fit in `kInlineSize` bytes (and can be moved without
throwing) are constructed directly *inside* the `UniqueFunction` with placement new. `std::string`
does the same for short strings, which is why `sizeof(std::string)` is 32 bytes.

Why `std::function` rejects move-only lambdas: it is **copyable**, so whatever it holds must be copyable too.
C++23 added `std::move_only_function` for exactly this; Gringofts is on C++17, so you'd write this class yourself.

## Tasks (`src/unique_function.h`)
`kHeapOps` (heap mode) is given as a worked example. Implement:
1. `kInlineOps`: invoke in place, move-construct-then-destroy, destroy (ex 18's placement new / explicit destructor skills).
2. The converting constructor: pick inline vs heap with `if constexpr (fitsInline<Fn>())`.
3. `moveFrom` and `reset`.

## Experiments
- Remove the `std::is_nothrow_move_constructible_v` condition from `fitsInline` and explain why
  `UniqueFunction`'s move constructor could then no longer be `noexcept`.
- Print `sizeof(UniqueFunction<void()>)` and `sizeof(std::function<void()>)`. Raise `kInlineSize`: what's the trade-off?
- Make `invoke` a `virtual` function on a base class instead of a function-pointer table. What extra allocation appears?
