# 18 — Build your own `std::vector`

**Time:** 3 h · **Part 3: design fluency.** This is the single best exercise for understanding
C++ object lifetime. After it, ex 02, 03 and crash labs 06 and 07 will feel obvious.

## Why
In Java an `ArrayList` holds references, and "creating" an element and "having memory for it" are
the same thing. A C++ `vector<T>` holds the objects themselves, so it must separate:
- **raw memory** (`capacity`): bytes from `::operator new`, no objects in them yet;
- **live objects** (`size`): created with **placement new** `new (ptr) T(args...)`, destroyed with an
  **explicit destructor call** `ptr->~T()`, never with `delete`.

Mixing these up is exactly how use-after-free, double-destroy and leaks happen.

## Exception-safety vocabulary (you'll use it in code reviews)
- **Basic guarantee:** if an operation throws, nothing leaks and the object is still valid.
- **Strong guarantee:** if it throws, the object is **unchanged** (commit-or-rollback).
- **Nothrow:** it can't throw (`noexcept`).

`reserve` must give the strong guarantee. That's why it uses `std::move_if_noexcept`: if `T`'s move
constructor might throw, a half-moved buffer couldn't be rolled back, so it copies instead. This is
why ex 03 told you to mark move constructors `noexcept`.

## Tasks (`src/vector.h`)
`allocate`, `deallocate` and `destroyRange` are given. Implement, in this order:
1. destructor, `clear`, `pop_back`, `at`;
2. `reserve`, with the strong guarantee;
3. `emplace_back`, with geometric growth. Watch out for `v.push_back(v[0])` (there's a test for it);
4. `resize`, copy/move constructors and assignments (copy-and-swap).

## Experiments
- Replace `std::move_if_noexcept` with `std::move` and run the `StrongGuarantee` test. Why does it fail?
- Remove `noexcept` from `Tracked`'s move constructor in `test.cpp`: which test fails, and why is that correct behaviour?
- Run the ASan build and break `destroyRange` (skip the last element). Then try making it destroy one element too many.
- Compare with libstdc++'s `std::vector::_M_realloc_insert` (in `/usr/include/c++/*/bits/vector.tcc`). It's the same algorithm.
