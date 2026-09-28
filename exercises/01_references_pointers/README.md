# 01 — Values, references, pointers, const

**Time:** 1 h · **Gringofts link:** every function signature. Look at
`IncreaseHandler::process(const AppStateMachine &, const IncreaseCommand &, std::vector<std::shared_ptr<Event>> *events)`
in `src/app_demo/execution/IncreaseHandler.cpp`.

## The big mental shift
In Java, `Foo f = other;` copies a *reference*. In C++, `Foo f = other;`
**copies the whole object**. Objects are values by default.

| C++ parameter | Meaning | Java analogy |
|---|---|---|
| `Foo f` | a copy (or a move, see ex 03) | primitive pass-by-value |
| `const Foo &f` | read-only alias, no copy — **the default for objects** | passing an object you promise not to mutate |
| `Foo &f` | mutable alias, caller's object changes | — (Java can't rebind caller's variable) |
| `Foo *f` | nullable address; may be reassigned | a nullable Java reference |
| `const Foo *f` | pointer to read-only Foo | — |

Gringofts convention (from the Google style guide it follows): **inputs** are
values or `const &`; **outputs** are pointers (`std::vector<...> *events`) so the
call site reads `process(state, cmd, &events)` and the `&` shouts "this is modified".

## Tasks (`src/refs.h`)
1. `increment(int &x)` — add 1 to the caller's variable.
2. `swapValues(int &a, int &b)`.
3. `sumAll(const std::vector<int> &v)` — why is `const &` better than by-value?
4. `tryParse(const std::string &s, int *out)` — Gringofts-style output param.
   Return `false` (and leave `*out` untouched) if `s` isn't a valid integer or
   `out == nullptr`. Hint: `std::from_chars` or `std::stoi` + try/catch.
5. `longest(const std::vector<std::string> &v)` returns `const std::string &`
   — a reference **into the vector** (no copy). Throw `std::invalid_argument`
   on empty input. The test checks the returned address is the element's address.
6. `appendEvent(std::vector<std::string> *events, const std::string &e)`.

## Think about
- What happens if `longest` returned a reference to a local variable? (Compile
  with `-Wall`: `returning reference to local`. In Java this can't happen; in
  C++ it's undefined behaviour — a *dangling reference*.)
- Why must `const std::string &longest(...)` not be called with a temporary
  vector and the result stored as a reference? `const auto &x = longest({"a"});` dangles.
