# 24 — Events as `std::variant`: closed sets, `std::visit`, exhaustiveness

**Time:** 2 h · **Part 3.** Rewrite ex 12's event-sourced counter with value types in a `std::variant`
instead of a class hierarchy. Then compare the two designs. Both appear in modern C++ codebases, and you
should be able to argue for either in a design review.

## The two designs
| | Virtual hierarchy (ex 05, ex 12, Gringofts' `Event`) | `std::variant` (this exercise) |
|---|---|---|
| Adding a new **type** | easy: add a subclass, nothing else changes | every `visit` must handle it (the compiler finds them all) |
| Adding a new **operation** | touch every subclass (or add a visitor pattern) | easy: write one more `visit` |
| Storage | one heap object per event, behind a pointer | the value itself, inline, contiguous in a `vector` |
| Missing case | compiles; you find out at run time | **compile error**: exhaustiveness checking |
| Open to plugins/other modules | yes | no: the set is closed at compile time |

(This trade-off is called the *expression problem*.) Gringofts uses a hierarchy because applications define their own
event types on top of the framework, an **open** set. Inside one application, a closed set of events is a strong
candidate for `variant`. Java 21's sealed interfaces + pattern-matching `switch` are the same idea.

## The `overloaded` idiom
```cpp
template <typename... Fs> struct overloaded : Fs... { using Fs::operator()...; };
template <typename... Fs> overloaded(Fs...) -> overloaded<Fs...>;   // C++17 deduction guide

std::visit(overloaded{
    [](const Increased &e) { ... },
    [](const Decreased &e) { ... },
    [](const Reset &)      { ... },
}, event);
```
It builds one type that inherits a call operator from every lambda, so overload resolution picks
the right one for whichever alternative the variant holds.

## Tasks (`src/variant_events.h`)
1. `StateMachine::process` and `apply` with `std::visit` + `overloaded`.
2. `encode`/`decode` using `variant::index()` as the type tag. **The order of the alternatives is now a
   persisted format** (ex 14's rule again): append new alternatives at the end, never reorder.

## Experiments
- Add `struct Doubled { int newValue; };` to `Event` and build: `apply`'s `overloaded` visitor fails to compile
  until you handle it. That's the feature. Notice that `encode`'s *generic* lambda compiles silently (it treats
  `Doubled` like any type with a `newValue`), and `decode`'s `switch` doesn't know index 3. When is a generic
  fallback convenient, and when does it hide a bug?
- Run the `CompareWithVirtualDispatch` test in a Release build (`-DCMAKE_BUILD_TYPE=Release`) and explain the difference
  (allocations, pointer chasing, cache misses, inlining).
- `std::get<T>` on the wrong alternative throws `std::bad_variant_access`; `std::get_if<T>` returns `nullptr`. When would you use each?
