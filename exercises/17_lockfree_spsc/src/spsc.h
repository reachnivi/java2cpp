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
  // TODO: producer side.
  //   1. read mTail (relaxed: only this thread writes it)
  //   2. read mHead with acquire (see how far the consumer got)
  //   3. full if tail - head == N -> return false
  //   4. write the slot mSlots[tail & (N - 1)] (std::move the value)
  //   5. publish: mTail.store(tail + 1, release)
  bool tryPush(T value) {
    (void)value;
    return false;
  }

  // TODO: consumer side, the mirror image: relaxed load of mHead, acquire
  // load of mTail, empty if equal, move the slot out, release-store head + 1.
  std::optional<T> tryPop() { return std::nullopt; }

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
