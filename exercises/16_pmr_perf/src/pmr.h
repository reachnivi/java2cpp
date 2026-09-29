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
    // TODO: count the allocation (mAllocations, mOutstanding, mPeak)
    return mUpstream->allocate(bytes, alignment);
  }

  void do_deallocate(void *p, size_t bytes, size_t alignment) override {
    // TODO: count the deallocation
    mUpstream->deallocate(p, bytes, alignment);
  }

  // TODO: two resources are interchangeable only if one can free the other's
  // memory. For a stateful resource like this, that means "same object".
  bool do_is_equal(const std::pmr::memory_resource &other) const noexcept override {
    (void)other;
    return true;  // TODO: wrong on purpose
  }

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
  // TODO: construct the vector with `mr`, reserve(n), then emplace_back n
  // strings of payloadSize copies of ('a' + i % 26). Check (test) that the
  // inner strings picked up `mr` automatically — why do they?
  (void)n;
  (void)payloadSize;
  return EventBatch(mr);
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
