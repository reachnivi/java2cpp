#ifndef J2C_LRU_CACHE_H_
#define J2C_LRU_CACHE_H_

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace j2c {

// O(1) LRU cache. Java: LinkedHashMap(accessOrder = true) + removeEldestEntry.
// The list holds entries in recency order (front = most recent). The map
// points INTO the list: std::list iterators stay valid when other nodes are
// inserted, erased or spliced, which is the whole trick.
template <typename K, typename V>
class LruCache {
 public:
  explicit LruCache(size_t capacity) : mCapacity(capacity) {
    if (capacity == 0) {
      throw std::invalid_argument("LruCache capacity must be > 0");
    }
  }

  // Returns the value and marks the key most-recently used.
  std::optional<V> get(const K &key) {
    // TODO: find in mIndex; if found, move its list node to the front with
    // mEntries.splice(mEntries.begin(), mEntries, nodeIterator) (O(1), and
    // every iterator stays valid), then return the value.
    (void)key;
    return std::nullopt;
  }

  // Inserts or updates; evicts the least-recently used entry if over capacity.
  // Returns the evicted key, if any.
  std::optional<K> put(const K &key, V value) {
    // TODO: existing key -> update value + splice to front.
    // New key -> emplace_front into mEntries, index it, and if over capacity
    // evict mEntries.back() (from the map FIRST, while you still have its key).
    (void)key;
    (void)value;
    return std::nullopt;
  }

  bool erase(const K &key) {
    // TODO: remove from both structures.
    (void)key;
    return false;
  }

  bool contains(const K &key) const { return mIndex.count(key) != 0; }
  size_t size() const { return mEntries.size(); }
  size_t capacity() const { return mCapacity; }

  // Keys from most- to least-recently used (for tests and debugging).
  template <typename Out>
  void keysByRecency(Out out) const {
    for (const auto &e : mEntries) {
      *out++ = e.first;
    }
  }

 private:
  using Entry = std::pair<K, V>;
  size_t mCapacity;
  std::list<Entry> mEntries;
  std::unordered_map<K, typename std::list<Entry>::iterator> mIndex;
};

// Thread-safe wrapper. Note get() MUTATES (it reorders the list), so even
// "reads" need the exclusive lock: a shared_mutex would be a bug here.
template <typename K, typename V>
class ConcurrentLru {
 public:
  explicit ConcurrentLru(size_t capacity) : mCache(capacity) {}

  std::optional<V> get(const K &key) {
    // TODO: lock mMutex. Note get() reorders the list, so it needs the
    // EXCLUSIVE lock even though it looks like a read. Count a hit or a miss.
    return mCache.get(key);
  }

  void put(const K &key, V value) {
    // TODO: lock
    mCache.put(key, std::move(value));
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mCache.size();
  }
  size_t hits() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mHits;
  }
  size_t misses() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mMisses;
  }

 private:
  mutable std::mutex mMutex;
  LruCache<K, V> mCache;
  size_t mHits = 0;
  size_t mMisses = 0;
};

}  // namespace j2c

#endif  // J2C_LRU_CACHE_H_
