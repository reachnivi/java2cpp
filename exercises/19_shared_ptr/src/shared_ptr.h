#ifndef J2C_SHARED_PTR_H_
#define J2C_SHARED_PTR_H_

#include <atomic>
#include <cstddef>
#include <new>
#include <utility>

namespace j2c {

// The control block: counts + how to destroy the object + how to free itself.
// Virtual functions give us type erasure: SharedPtr<T> doesn't need to know the
// deleter's type or whether the object lives inside the block.
class ControlBlockBase {
 public:
  virtual ~ControlBlockBase() = default;

  void addStrong() noexcept { mStrong.fetch_add(1, std::memory_order_relaxed); }
  void addWeak() noexcept { mWeak.fetch_add(1, std::memory_order_relaxed); }

  // The last strong owner destroys the object, then gives up the weak
  // count the strong owners hold collectively.
  void releaseStrong() noexcept {
    // TODO: fetch_sub with memory_order_acq_rel. If this was the LAST strong
    // reference: destroyObject(), then releaseWeak() (the strong owners
    // collectively hold one weak count; see mWeak's initial value).
  }

  void releaseWeak() noexcept {
    // TODO: if this was the last weak reference, destroySelf().
  }

  // WeakPtr::lock(): increment only if the object is still alive.
  bool tryAddStrong() noexcept {
    // TODO: a compare_exchange_weak loop. Never increment from 0: a dead
    // object must stay dead. (Why can't you just fetch_add and check?)
    return false;
  }

  long strongCount() const noexcept { return mStrong.load(std::memory_order_relaxed); }

 protected:
  virtual void destroyObject() noexcept = 0;
  virtual void destroySelf() noexcept { delete this; }

 private:
  std::atomic<long> mStrong{1};
  std::atomic<long> mWeak{1};  // 1 on behalf of all strong owners
};

// Block for SharedPtr<T>(T*, Deleter): the object lives elsewhere.
template <typename T, typename Deleter>
class PointerControlBlock final : public ControlBlockBase {
 public:
  PointerControlBlock(T *p, Deleter d) : mPtr(p), mDeleter(std::move(d)) {}

 protected:
  void destroyObject() noexcept override { mDeleter(mPtr); }

 private:
  T *mPtr;
  Deleter mDeleter;
};

// Block for makeShared: the object is constructed INSIDE the block, so one
// allocation holds both. Destroying the object and freeing the block are
// separate steps: weak pointers can keep the block (not the object) alive.
template <typename T>
class InlineControlBlock final : public ControlBlockBase {
 public:
  template <typename... Args>
  explicit InlineControlBlock(Args &&...args) {
    // TODO: placement-new the T into mStorage (ex 18 skills).
    ((void)args, ...);
  }
  T *object() noexcept { return std::launder(reinterpret_cast<T *>(&mStorage)); }

 protected:
  void destroyObject() noexcept override {
    // TODO: explicit destructor call (NOT delete: the memory is the block's).
  }

 private:
  alignas(T) unsigned char mStorage[sizeof(T)];
};

template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr {
 public:
  SharedPtr() noexcept = default;
  SharedPtr(std::nullptr_t) noexcept {}  // NOLINT: implicit on purpose, like std::shared_ptr

  explicit SharedPtr(T *p) : SharedPtr(p, [](T *q) { delete q; }) {}

  template <typename Deleter>
  SharedPtr(T *p, Deleter d) : mPtr(p) {
    // TODO: mBlock = new PointerControlBlock<T, Deleter>(p, d);
    // If that `new` throws, you still own p: call d(p) before rethrowing.
    (void)d;
  }

  SharedPtr(const SharedPtr &other) noexcept : mPtr(other.mPtr), mBlock(other.mBlock) {
    // TODO: one more strong owner
  }
  SharedPtr(SharedPtr &&other) noexcept : mPtr(other.mPtr), mBlock(other.mBlock) {
    // TODO: steal instead of sharing (std::exchange); the count doesn't change.
  }

  SharedPtr &operator=(SharedPtr other) noexcept {  // by value: covers copy and move
    // TODO: copy-and-swap. Why is self-assignment automatically safe here?
    (void)other;
    return *this;
  }

  ~SharedPtr() {
    // TODO: release one strong reference (if we hold a block).
  }

  void swap(SharedPtr &other) noexcept {
    std::swap(mPtr, other.mPtr);
    std::swap(mBlock, other.mBlock);
  }
  void reset() noexcept { SharedPtr().swap(*this); }

  T *get() const noexcept { return mPtr; }
  T &operator*() const noexcept { return *mPtr; }
  T *operator->() const noexcept { return mPtr; }
  explicit operator bool() const noexcept { return mPtr != nullptr; }
  long use_count() const noexcept { return mBlock == nullptr ? 0 : mBlock->strongCount(); }

  // Test hook: where the control block lives (for the one-allocation check).
  const void *controlBlockAddress() const noexcept { return mBlock; }

 private:
  template <typename U, typename... Args>
  friend SharedPtr<U> makeShared(Args &&...args);
  friend class WeakPtr<T>;

  // Adopts a reference already counted in `block`.
  SharedPtr(T *p, ControlBlockBase *block) noexcept : mPtr(p), mBlock(block) {}

  T *mPtr = nullptr;
  ControlBlockBase *mBlock = nullptr;
};

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args &&...args) {
  // TODO: ONE allocation: new InlineControlBlock<T>(args...), then adopt it
  // with the private SharedPtr(T*, ControlBlockBase*) constructor.
  // Trap: pass the block as static_cast<ControlBlockBase *>(block). Without
  // the cast, which constructor does overload resolution pick, and why?
  ((void)args, ...);
  return SharedPtr<T>();
}

template <typename T>
class WeakPtr {
 public:
  WeakPtr() noexcept = default;
  WeakPtr(const SharedPtr<T> &s) noexcept : mPtr(s.mPtr), mBlock(s.mBlock) {  // NOLINT: implicit like std
    // TODO: one more weak owner
  }
  WeakPtr(const WeakPtr &other) noexcept : mPtr(other.mPtr), mBlock(other.mBlock) {
    // TODO: one more weak owner
  }
  WeakPtr(WeakPtr &&other) noexcept
      : mPtr(std::exchange(other.mPtr, nullptr)), mBlock(std::exchange(other.mBlock, nullptr)) {}
  WeakPtr &operator=(WeakPtr other) noexcept {
    std::swap(mPtr, other.mPtr);
    std::swap(mBlock, other.mBlock);
    return *this;
  }
  ~WeakPtr() {
    // TODO: release one weak reference
  }

  SharedPtr<T> lock() const noexcept {
    // TODO: if tryAddStrong() succeeds, adopt that reference into a SharedPtr.
    return SharedPtr<T>();
  }
  bool expired() const noexcept { return mBlock == nullptr || mBlock->strongCount() == 0; }

 private:
  T *mPtr = nullptr;
  ControlBlockBase *mBlock = nullptr;
};

}  // namespace j2c

#endif  // J2C_SHARED_PTR_H_
