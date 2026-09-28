# 03 — Copy vs move, the Rule of 0/3/5

**Time:** 1.5 h · **Gringofts link:** `std::move` appears ~100 times; look at how
`std::shared_ptr<Command>` is moved into queues, and `DoubleBuffer.h` in `src/infra/raft/`.

## Why
Because objects are values, C++ must know how to **copy** them. Since C++11 it
can also **move** them: steal the guts of an object that's about to die instead
of deep-copying. `std::move(x)` doesn't move anything — it's a cast that says
"I'm done with `x`, you may pillage it".

The special members:
```cpp
Buffer(const Buffer &);             // copy ctor
Buffer &operator=(const Buffer &);  // copy assignment
Buffer(Buffer &&) noexcept;         // move ctor
Buffer &operator=(Buffer &&) noexcept; // move assignment
~Buffer();                          // destructor
```
- **Rule of 0** (preferred): hold only members that already manage themselves
  (`std::string`, `std::vector`, `std::unique_ptr`) and write *none* of these.
- **Rule of 5**: if you own a raw resource and write one, write all five.

## Tasks (`src/buffer.h`)
Implement `Buffer`, which owns a raw `new char[]` array (on purpose — in real
code you'd use `std::vector<char>` and follow the Rule of 0).
1. ctor `Buffer(std::size_t size)` allocates and zero-fills; bumps `allocations()`.
2. Destructor frees (`delete[]`).
3. Copy ctor / copy assignment: **deep copy** (bumps `allocations()`). Handle
   self-assignment `b = b;`.
4. Move ctor / move assignment: steal pointer + size, leave source as
   `data()==nullptr, size()==0`. **No allocation.** Mark them `noexcept`.
5. Finally, write `BufferRule0` using `std::vector<char>` and no special
   members at all — note how the same tests pass with 5 lines of code.

## Think about
- Why does `std::vector<Buffer>` only use your move constructor on growth if it is
  `noexcept`? (Strong exception guarantee.) The test checks this.
- After `Buffer b2 = std::move(b1);` what may you still do with `b1`? (Destroy
  or assign to it. Don't read it.)
- Return values: `Buffer make() { Buffer b(10); return b; }` neither copies nor
  moves thanks to copy elision / NRVO. Never write `return std::move(b);`.
