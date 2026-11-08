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
    // acq_rel: every other owner's writes to the object happen-before the
    // destroy that the last owner performs.
    if (mStrong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      destroyObject();
      releaseWeak();  // the strong owners collectively hold one weak count
    }
  }

  void releaseWeak() noexcept {
    if (mWeak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      destroySelf();
    }
  }

  // WeakPtr::lock(): increment only if the object is still alive.
  bool tryAddStrong() noexcept {
    long n = mStrong.load(std::memory_order_relaxed);
    while (n != 0) {
      if (mStrong.compare_exchange_weak(n, n + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
        return true;
      }
    }
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
    new (&mStorage) T(std::forward<Args>(args)...);
  }
  T *object() noexcept { return std::launder(reinterpret_cast<T *>(&mStorage)); }

 protected:
  void destroyObject() noexcept override { object()->~T(); }

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
    try {
      mBlock = new PointerControlBlock<T, Deleter>(p, d);
    } catch (...) {
      d(p);  // we were handed ownership: don't leak it if the block can't be allocated
      throw;
    }
  }

  SharedPtr(const SharedPtr &other) noexcept : mPtr(other.mPtr), mBlock(other.mBlock) {
    if (mBlock != nullptr) {
      mBlock->addStrong();
    }
  }
  SharedPtr(SharedPtr &&other) noexcept
      : mPtr(std::exchange(other.mPtr, nullptr)), mBlock(std::exchange(other.mBlock, nullptr)) {}

  SharedPtr &operator=(SharedPtr other) noexcept {  // by value: covers copy and move
    swap(other);
    return *this;
  }

  ~SharedPtr() {
    if (mBlock != nullptr) {
      mBlock->releaseStrong();
    }
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
  auto *block = new InlineControlBlock<T>(std::forward<Args>(args)...);
  // The cast matters: with an InlineControlBlock<T>* argument, the template
  // constructor SharedPtr(T*, Deleter) is an EXACT match (Deleter deduced as
  // the block pointer) and beats this one, which needs a derived-to-base
  // conversion. Overload resolution prefers exact template matches.
  return SharedPtr<T>(block->object(), static_cast<ControlBlockBase *>(block));
}

template <typename T>
class WeakPtr {
 public:
  WeakPtr() noexcept = default;
  WeakPtr(const SharedPtr<T> &s) noexcept : mPtr(s.mPtr), mBlock(s.mBlock) {  // NOLINT: implicit like std
    if (mBlock != nullptr) {
      mBlock->addWeak();
    }
  }
  WeakPtr(const WeakPtr &other) noexcept : mPtr(other.mPtr), mBlock(other.mBlock) {
    if (mBlock != nullptr) {
      mBlock->addWeak();
    }
  }
  WeakPtr(WeakPtr &&other) noexcept
      : mPtr(std::exchange(other.mPtr, nullptr)), mBlock(std::exchange(other.mBlock, nullptr)) {}
  WeakPtr &operator=(WeakPtr other) noexcept {
    std::swap(mPtr, other.mPtr);
    std::swap(mBlock, other.mBlock);
    return *this;
  }
  ~WeakPtr() {
    if (mBlock != nullptr) {
      mBlock->releaseWeak();
    }
  }

  SharedPtr<T> lock() const noexcept {
    if (mBlock != nullptr && mBlock->tryAddStrong()) {
      return SharedPtr<T>(mPtr, mBlock);
    }
    return SharedPtr<T>();
  }
  bool expired() const noexcept { return mBlock == nullptr || mBlock->strongCount() == 0; }

 private:
  T *mPtr = nullptr;
  ControlBlockBase *mBlock = nullptr;
};

}  // namespace j2c

#endif  // J2C_SHARED_PTR_H_
