#ifndef J2C_SPSC_H_
#define J2C_SPSC_H_

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <utility>

namespace j2c::lockfree {

// Single-producer / single-consumer ring buffer. Exactly one thread may call
// tryPush and exactly one (other) thread may call tryPop.
//
// mHead: next slot to read  (written only by the consumer)
// mTail: next slot to write (written only by the producer)
// Both only ever increase; slot = counter % N. Full when tail - head == N.
template <typename T, size_t N>
class SpscRing {
  static_assert(N > 0 && (N & (N - 1)) == 0, "N must be a power of two");

 public:
  bool tryPush(T value) {
    const size_t tail = mTail.load(std::memory_order_relaxed);  // only we write it
    const size_t head = mHead.load(std::memory_order_acquire);  // see consumer's progress
    if (tail - head == N) {
      return false;  // full
    }
    mSlots[tail & (N - 1)] = std::move(value);
    // release: the slot write above becomes visible before the new tail does.
    mTail.store(tail + 1, std::memory_order_release);
    return true;
  }

  std::optional<T> tryPop() {
    const size_t head = mHead.load(std::memory_order_relaxed);  // only we write it
    const size_t tail = mTail.load(std::memory_order_acquire);  // pairs with the producer's release
    if (head == tail) {
      return std::nullopt;  // empty
    }
    T value = std::move(mSlots[head & (N - 1)]);
    // release: we're done reading the slot before the producer may reuse it.
    mHead.store(head + 1, std::memory_order_release);
    return value;
  }

  size_t sizeApprox() const {
    return mTail.load(std::memory_order_acquire) - mHead.load(std::memory_order_acquire);
  }
  static constexpr size_t capacity() { return N; }

 private:
  // Separate cache lines: the producer hammers mTail and the consumer hammers
  // mHead. On the same line, every write would steal the line from the other
  // core ("false sharing").
  alignas(64) std::atomic<size_t> mHead{0};
  alignas(64) std::atomic<size_t> mTail{0};
  alignas(64) std::array<T, N> mSlots{};
};

}  // namespace j2c::lockfree

#endif  // J2C_SPSC_H_
