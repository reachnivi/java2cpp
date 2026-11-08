#ifndef J2C_UNIQUE_FUNCTION_H_
#define J2C_UNIQUE_FUNCTION_H_

#include <cstddef>
#include <functional>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace j2c {

template <typename Signature>
class UniqueFunction;

// A move-only std::function. Two ideas:
//  1. Type erasure WITHOUT virtual functions: a small table of function
//     pointers (invoke / move / destroy) generated per callable type.
//  2. Small-buffer optimisation (SBO): callables that fit in kInlineSize bytes
//     and can be moved without throwing live inside the object, no heap.
template <typename R, typename... Args>
class UniqueFunction<R(Args...)> {
 public:
  static constexpr size_t kInlineSize = 3 * sizeof(void *);

  UniqueFunction() noexcept = default;
  UniqueFunction(std::nullptr_t) noexcept {}  // NOLINT: implicit like std::function

  template <typename F, typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, UniqueFunction> &&
                                                    std::is_invocable_r_v<R, std::decay_t<F> &, Args...>>>
  UniqueFunction(F &&f) {  // NOLINT: implicit like std::function
    using Fn = std::decay_t<F>;
    // TODO: if constexpr (fitsInline<Fn>()): placement-new Fn into mStorage
    // and point mOps at kInlineOps<Fn>; else heap-allocate (new Fn(...)),
    // store the POINTER in mStorage, and use kHeapOps<Fn>.
    // Always std::forward<F>(f): the callable may be move-only.
    (void)f;
    (void)sizeof(Fn);
  }

  UniqueFunction(UniqueFunction &&other) noexcept { moveFrom(other); }

  UniqueFunction &operator=(UniqueFunction &&other) noexcept {
    if (this != &other) {
      reset();
      moveFrom(other);
    }
    return *this;
  }

  UniqueFunction(const UniqueFunction &) = delete;
  UniqueFunction &operator=(const UniqueFunction &) = delete;

  ~UniqueFunction() { reset(); }

  R operator()(Args... args) {
    if (mOps == nullptr) {
      throw std::bad_function_call();
    }
    return mOps->invoke(&mStorage, std::forward<Args>(args)...);
  }

  explicit operator bool() const noexcept { return mOps != nullptr; }
  bool isInline() const noexcept { return mOps != nullptr && mOps->isInline; }

 private:
  struct Ops {
    R (*invoke)(void *storage, Args &&...args);
    void (*moveTo)(void *from, void *to) noexcept;  // move-construct into `to`, destroy `from`
    void (*destroy)(void *storage) noexcept;
    bool isInline;
  };

  template <typename Fn>
  static constexpr bool fitsInline() {
    return sizeof(Fn) <= kInlineSize && alignof(Fn) <= alignof(std::max_align_t) &&
           std::is_nothrow_move_constructible_v<Fn>;
  }

  template <typename Fn>
  static Fn *inlinePtr(void *s) noexcept {
    return std::launder(reinterpret_cast<Fn *>(s));
  }
  template <typename Fn>
  static Fn *&heapPtr(void *s) noexcept {
    return *reinterpret_cast<Fn **>(s);
  }

  // TODO: the table for callables stored INSIDE mStorage. Use inlinePtr<Fn>(s).
  //   invoke : call the object in place
  //   moveTo : move-construct a new Fn at `to` from the one at `from`, then
  //            destroy the one at `from` (explicit destructor call)
  //   destroy: explicit destructor call
  template <typename Fn>
  static inline const Ops kInlineOps{
      [](void *, Args &&...) -> R { throw std::logic_error("TODO: kInlineOps.invoke"); },
      [](void *, void *) noexcept {},
      [](void *) noexcept {},
      true,
  };

  // Given, as a worked example: callables too big for the buffer live on the
  // heap, and mStorage just holds the pointer.
  template <typename Fn>
  static inline const Ops kHeapOps{
      [](void *s, Args &&...args) -> R { return std::invoke(*heapPtr<Fn>(s), std::forward<Args>(args)...); },
      [](void *from, void *to) noexcept {
        heapPtr<Fn>(to) = heapPtr<Fn>(from);  // just move the pointer
        heapPtr<Fn>(from) = nullptr;
      },
      [](void *s) noexcept { delete heapPtr<Fn>(s); },
      false,
  };

  void moveFrom(UniqueFunction &other) noexcept {
    // TODO: if other holds something: other's ops move its storage into ours,
    // then take its ops pointer and leave it empty (mOps == nullptr).
    (void)other;
  }

  void reset() noexcept {
    // TODO: destroy whatever we hold (through mOps) and become empty.
  }

  alignas(std::max_align_t) unsigned char mStorage[kInlineSize];
  const Ops *mOps = nullptr;
};

}  // namespace j2c

#endif  // J2C_UNIQUE_FUNCTION_H_
