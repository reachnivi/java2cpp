# 21 — An O(1) LRU cache: iterator stability, then thread safety

**Time:** 2 h · **Part 3.** Java gives you `LinkedHashMap(capacity, 0.75f, true)` plus `removeEldestEntry`.
In C++ you build it, and doing so teaches the one STL property that matters most in practice:
**which operations invalidate which iterators**.

## Design
```
mEntries : std::list<pair<K,V>>   front = most recent ... back = least recent
mIndex   : unordered_map<K, list::iterator>   key -> its node in the list
```
- `get`: look up the node, `splice` it to the front. That's O(1), and it moves the node without copying or reallocating.
- `put` (new key): `emplace_front`, index it; if over capacity, evict `back()`.

This only works because **`std::list` iterators stay valid** when *other* nodes are inserted, erased or
spliced. Try it with `std::vector` and every insertion could invalidate every stored iterator
(crash lab 06). Cheat sheet:

| Container | insert | erase |
|---|---|---|
| `vector` | invalidates all if it reallocates, else those after the point | those at/after the point |
| `deque` | all iterators (references survive at the ends) | depends on position |
| `list`, `map`, `set` | none | only the erased element |
| `unordered_map` | all **iterators** if it rehashes (references survive) | only the erased element |

## Thread safety
`ConcurrentLru` wraps the cache in a mutex. The trap: `get` **mutates** (it reorders the list), so
a reader–writer lock (`std::shared_mutex`) taken in shared mode for `get` would be a data race.
"Logically const" isn't "physically const".

## Tasks (`src/lru_cache.h`)
1. `get`, `put`, `erase` (all O(1)).
2. `ConcurrentLru`: add the locking and the hit/miss counters. Run `ex21` in the TSan build.

## Experiments
- Replace `splice` with "erase the node and push_front a copy". What breaks in `mIndex`?
- Use `std::shared_mutex` with `shared_lock` in `ConcurrentLru::get`. Run under TSan and read the report.
- For a hot cache, one mutex becomes the bottleneck. Sketch a **sharded** version (N caches, key hash → shard).
