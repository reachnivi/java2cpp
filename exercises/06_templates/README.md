# 06 — Templates (≠ Java generics)

**Time:** 1.5 h · **Gringofts link:** `src/infra/mpscqueue/MpscQueue.h`
(`template<typename T> class MpscQueue`), `src/app_util/CommandProcessLoop.h`
(`template <typename StateMachineType>`), and the `.hpp` files included at the
bottom of headers, e.g. `domain/CommandProcessLoop.h` → `CommandProcessLoop.hpp`.

## Key differences from Java generics
| Java generics | C++ templates |
|---|---|
| type erasure; one compiled class | **code generated per type** (`vector<int>` and `vector<string>` are different classes) |
| `T` must be a reference type | `T` can be `int`, a struct, anything |
| constraints via `<T extends Foo>` checked up front | "duck typing" checked at instantiation; C++20 adds `concepts` (Gringofts is C++17, so you'll see `static_assert`/`std::enable_if`) |
| lives in `.java` | **definitions must be visible where used → usually header-only** (hence Gringofts' `.hpp` files included from `.h`) |
| no value parameters | non-type params: `template<typename T, std::size_t N>` |

Template error messages are long. Read **the first error** and look for
"required from here" to find *your* line.

## Tasks (`src/templates.h`)
1. `RingBuffer<T, N>` — fixed capacity `N` backed by `std::array<T, N>`.
   `bool push(T value)` (false when full), `std::optional<T> pop()` (FIFO,
   `std::nullopt` when empty), `size()`, `empty()`, `full()`, `capacity()`
   (`static constexpr`).
2. `countIf(const Container &c, Pred pred)` — generic over any container
   with begin/end and any callable (lambda, function pointer, functor).
3. `describe(const T &v)` using `if constexpr`:
   integral → `"int:<v>"`, floating point → `"float"`,
   `std::string` → `"string:<v>"`, anything else → `"other"`.
   Use `<type_traits>`: `std::is_integral_v<T>`, `std::is_floating_point_v<T>`,
   `std::is_same_v<T, std::string>`. Note `bool` is integral!
4. `maxOf(a, b, rest...)` — a **variadic template** returning the max of 2+ args.
   (C++17 fold expressions or recursion.)

## Experiments
- Call `countIf(42, ...)` and read the error. Find the "required from here" line.
- Put `RingBuffer`'s method definitions in a `.cpp` and watch the linker fail.
