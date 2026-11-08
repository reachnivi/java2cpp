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
    auto it = mIndex.find(key);
    if (it == mIndex.end()) {
      return std::nullopt;
    }
    mEntries.splice(mEntries.begin(), mEntries, it->second);  // O(1), iterators stay valid
    return it->second->second;
  }

  // Inserts or updates; evicts the least-recently used entry if over capacity.
  // Returns the evicted key, if any.
  std::optional<K> put(const K &key, V value) {
    auto it = mIndex.find(key);
    if (it != mIndex.end()) {
      it->second->second = std::move(value);
      mEntries.splice(mEntries.begin(), mEntries, it->second);
      return std::nullopt;
    }
    mEntries.emplace_front(key, std::move(value));
    mIndex.emplace(key, mEntries.begin());
    if (mEntries.size() > mCapacity) {
      K evicted = std::move(mEntries.back().first);
      mIndex.erase(evicted);
      mEntries.pop_back();
      return evicted;
    }
    return std::nullopt;
  }

  bool erase(const K &key) {
    auto it = mIndex.find(key);
    if (it == mIndex.end()) {
      return false;
    }
    mEntries.erase(it->second);
    mIndex.erase(it);
    return true;
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
    std::lock_guard<std::mutex> lock(mMutex);
    auto v = mCache.get(key);
    ++(v ? mHits : mMisses);
    return v;
  }

  void put(const K &key, V value) {
    std::lock_guard<std::mutex> lock(mMutex);
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
