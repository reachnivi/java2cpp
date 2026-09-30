# 07 — STL containers, algorithms, lambdas (your Streams replacement)

**Time:** 2 h · **Gringofts link:** `src/infra/util/ClusterInfo.cpp`,
`src/infra/raft/v2/RaftCore.cpp` (maps of peers, `std::sort`, lambdas passed to threads).

## Container map
| Java | C++ | Notes |
|---|---|---|
| `ArrayList` | `std::vector` | the default; contiguous; `reserve()` to pre-size |
| `LinkedList` / `ArrayDeque` | `std::list` / `std::deque` | |
| `HashMap` | `std::unordered_map` | `m[k]` **inserts a default** if missing! use `find`/`count`/`at` to read |
| `TreeMap` | `std::map` | ordered; iteration is sorted by key |
| `HashSet` / `TreeSet` | `std::unordered_set` / `std::set` | |
| `Optional` | `std::optional` | ex 08 |
| `stream().filter().map().collect()` | `std::copy_if`, `std::transform`, loops | C++20 has `ranges`; Gringofts is C++17, so: algorithms + plain loops |

Iterator invalidation is the #1 STL bug for Java devs: `push_back` on a
vector may reallocate and invalidate every pointer/reference/iterator into it.
Erasing while iterating needs the **erase–remove idiom** or `it = m.erase(it)`.

## Lambdas
```cpp
[captures](params) -> ret { body }
[=]   // capture everything used, by copy
[&]   // capture everything used, by reference (danger if lambda outlives scope!)
[this] / [x, &y] / [p = std::move(ptr)]   // explicit — preferred in Gringofts
```
Java lambdas capture effectively-final values; C++ lambdas can capture
**references to locals** — if the lambda runs later on another thread, that's
a dangling reference.

## Tasks (`src/stl.h`) — given `struct Order { int id; std::string user; double amount; }`
1. `totalByUser(orders)` → `std::map<std::string, double>`.
2. `idsAbove(orders, threshold)` → `std::vector<int>` of ids with amount >
   threshold, **in original order** (`copy_if` + `transform`, or a loop).
3. `topN(orders, n)` → the `n` largest orders by amount, descending
   (`std::sort` or `std::partial_sort` with a lambda comparator).
4. `removeSmall(std::vector<Order> *orders, double min)` — in-place, erase–remove idiom.
5. `distinctUsersSorted(orders)` → sorted unique user names.
6. `makeCounter()` → `std::function<int()>` returning 1, 2, 3, ... on each
   call. (Capture by value + `mutable`. Each counter is independent.)
7. `countWords(text)` → `std::unordered_map<std::string, int>`
   (use `std::istringstream` to split on whitespace).
