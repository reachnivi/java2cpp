# Java → C++17 cheat sheet (Gringofts dialect)

## Mental model shifts
1. **Objects are values.** `Foo a = b;` copies. Use references (`Foo &`) or
   pointers to alias. Most objects live on the stack or inside other objects,
   not on a heap managed by a GC.
2. **Lifetime is your job.** Every object has an owner; when the owner goes
   out of scope, the destructor runs *immediately*. Express ownership with
   types: `std::unique_ptr` (one owner), `std::shared_ptr` (shared), `T&`/`T*` (borrowed).
3. **Undefined behaviour (UB)** is not an exception. Out-of-bounds, dangling
   references, data races, signed overflow — the program may appear to work,
   crash later, or corrupt data. Sanitizers find it; the compiler won't stop you.
4. **Compile-time over run-time.** Templates, `constexpr`, `static_assert`,
   and `const` catch errors earlier than Java generics or reflection.
5. **Headers vs sources.** Declarations in `.h`, definitions in `.cpp`,
   templates in headers (Gringofts puts large template bodies in `.hpp`).

## Syntax map
| Java | C++17 |
|---|---|
| `package a.b;` | `namespace a::b { ... }` |
| `import a.b.C;` | `#include "a/b/C.h"` + `using a::b::C;` (never `using namespace` in a header) |
| `final int X = 3;` (constant) | `static constexpr int kX = 3;` |
| `final Foo f` (can't reassign) | `const Foo f` — **also** makes the object immutable |
| `var x = ...` | `auto x = ...` (`auto` drops `const`/`&`; write `const auto &x = ...` to avoid a copy) |
| `String` | `std::string` (mutable value!) / `std::string_view` (non-owning view) |
| `int`, `long` | `int32_t`, `int64_t`, `uint64_t` (`<cstdint>`) — Gringofts uses fixed-width |
| `List<T>`/`ArrayList` | `std::vector<T>` |
| `Map<K,V>`/`HashMap` | `std::unordered_map<K,V>` |
| `TreeMap` | `std::map` |
| `Optional<T>` | `std::optional<T>` |
| `Object` / `instanceof` | no universal base; `dynamic_cast<T*>(p)` |
| `interface I { void f(); }` | `class I { public: virtual ~I() = default; virtual void f() = 0; };` |
| `class B extends A implements I` | `class B : public A, public I` |
| `@Override` | `override` |
| `super.f()` | `A::f()` |
| `this.x` | `this->x` or `mX` (Gringofts names members `mX`) |
| `static` nested class | nested class (no implicit outer reference, ever) |
| `enum` | `enum class Color { kRed, kGreen };` |
| `record Point(int x, int y)` | `struct Point { int x; int y; };` + `==` you write yourself (C++20 can default it; not C++17) |
| `new Foo()` | `std::make_unique<Foo>()` / `std::make_shared<Foo>()` — never bare `new` in app code |
| `null` | `nullptr` (pointers), `std::nullopt` (optional) |
| `x.equals(y)` | `x == y` (only if `operator==` is defined) |
| `hashCode()` | specialise `std::hash<T>` |
| `toString()` | `operator<<(std::ostream&, const T&)` or a `toString()` method |
| `try { } finally { }` | RAII destructor / `std::lock_guard` |
| `try (Res r = ...)` | just declare `Res r(...);` in a scope |
| `throws IOException` | nothing; document it. `noexcept` marks functions that must not throw |
| `synchronized` | `std::lock_guard<std::mutex> lock(mMutex);` |
| `AtomicLong` | `std::atomic<uint64_t>` |
| `Thread`, `join()` | `std::thread`, `join()` — mandatory before destruction |
| `ExecutorService`, `CompletableFuture` | not in std (C++17 has `std::async`/`std::future`, rarely used in Gringofts) |
| `Function<A,B>` / lambda | `std::function<B(A)>` / `[capture](A a) { ... }` |
| `stream().filter().map()` | `<algorithm>`: `std::copy_if`, `std::transform`, `std::sort`, or a loop |
| `logger.info("x={}", x)` | `SPDLOG_INFO("x={}", x);` |
| `Properties` | `INIReader reader("conf.ini"); reader.Get("section", "key", "default")` |
| JUnit / Mockito | gtest `TEST/TEST_F`, gmock `MOCK_METHOD`, `EXPECT_CALL` |
| Maven / Gradle | CMake (+ git submodules in Gringofts' `third_party/`) |

## Parameter passing rules (Google style, which Gringofts follows)
```cpp
void f(int n);                         // small/trivial types: by value
void f(const std::string &s);          // read-only object: const ref
void f(std::string s);                 // you'll keep a copy anyway: by value, then std::move(s) into place
void f(std::unique_ptr<Foo> p);        // taking ownership: by value, caller writes f(std::move(p))
void f(const std::shared_ptr<Foo> &p); // might share ownership: const ref (avoids refcount bump)
void f(Foo *out);                      // output parameter: pointer (call site: f(&x))
```

## `const` everywhere
```cpp
class Account {
 public:
  int64_t balance() const { return mBalance; }   // const method: can't modify members
  void deposit(int64_t amt);                      // non-const: may modify
 private:
  int64_t mBalance = 0;
  mutable std::mutex mMutex;                      // `mutable`: lockable inside const methods
};
const Account &a = getAccount();
a.balance();   // OK
a.deposit(1);  // compile error
```

## Things that compile but are bugs
```cpp
std::string_view v = std::string("tmp");           // dangles immediately
const std::string &r = map.at(k); map.insert(...); // may dangle if rehashed
for (auto x : bigVector) {}                         // copies every element: use const auto &
std::thread t(work); /* no join */                  // std::terminate() at scope exit
auto lambda = [&]{ return local; }; return lambda;  // captures dangling reference
class Base { public: ~Base(); };                    // non-virtual dtor + delete via Base* = UB
if (map[key] == 0)                                  // inserts key if missing
int i = vec.size() - 1;                             // size_t underflow when empty
std::shared_ptr<Foo>(this)                          // second control block → double delete; use enable_shared_from_this
```

## Reading compiler errors
- Scroll to the **first** error. Later ones are usually fallout.
- For templates, find `required from here` — that line is in *your* code.
- `use of deleted function` → you copied a move-only type (`unique_ptr`, `std::thread`, `std::mutex`). Add `std::move` or use a reference.
- `undefined reference to` → linker: missing `.cpp` in CMake target or missing library in `target_link_libraries`.
- `incomplete type` → you only have a forward declaration; `#include` the real header.
- `passing 'const X' as 'this' argument discards qualifiers` → calling a non-`const` method on a `const` object; mark the method `const` if it doesn't mutate.
