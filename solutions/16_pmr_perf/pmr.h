#ifndef J2C_PMR_H_
#define J2C_PMR_H_

#include <cstddef>
#include <memory_resource>
#include <string>
#include <vector>

namespace j2c::mem {

// A memory_resource that forwards to `upstream` and counts what passes
// through, like Gringofts' TrackingMemoryResource.
class CountingResource : public std::pmr::memory_resource {
 public:
  explicit CountingResource(std::pmr::memory_resource *upstream = std::pmr::new_delete_resource())
      : mUpstream(upstream) {}

  size_t allocations() const { return mAllocations; }
  size_t deallocations() const { return mDeallocations; }
  size_t bytesOutstanding() const { return mOutstanding; }
  size_t peakBytes() const { return mPeak; }

 protected:
  void *do_allocate(size_t bytes, size_t alignment) override {
    void *p = mUpstream->allocate(bytes, alignment);
    ++mAllocations;
    mOutstanding += bytes;
    if (mOutstanding > mPeak) {
      mPeak = mOutstanding;
    }
    return p;
  }

  void do_deallocate(void *p, size_t bytes, size_t alignment) override {
    mUpstream->deallocate(p, bytes, alignment);
    ++mDeallocations;
    mOutstanding -= bytes;
  }

  bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override { return this == &other; }

 private:
  std::pmr::memory_resource *mUpstream;
  size_t mAllocations = 0;
  size_t mDeallocations = 0;
  size_t mOutstanding = 0;
  size_t mPeak = 0;
};

using EventBatch = std::pmr::vector<std::pmr::string>;

// Build `n` encoded events, each `payloadSize` bytes, with ALL memory coming
// from `mr`: the vector's buffer and every string's buffer.
// Exactly one allocation for the vector (reserve), plus one per string that
// doesn't fit in the small-string buffer.
inline EventBatch makeEventBatch(size_t n, size_t payloadSize, std::pmr::memory_resource *mr) {
  EventBatch batch(mr);
  batch.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    // emplace_back uses uses-allocator construction: the string gets `mr` too.
    batch.emplace_back(payloadSize, static_cast<char>('a' + i % 26));
  }
  return batch;
}

// Same work with the default allocator, for comparison.
inline std::vector<std::string> makeEventBatchStd(size_t n, size_t payloadSize) {
  std::vector<std::string> batch;
  batch.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    batch.emplace_back(payloadSize, static_cast<char>('a' + i % 26));
  }
  return batch;
}

}  // namespace j2c::mem

#endif  // J2C_PMR_H_
