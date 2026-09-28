# 04 — Smart pointers: ownership made explicit

**Time:** 1.5 h · **Gringofts link:** `shared_ptr` ~260 uses, `unique_ptr` ~120.
See `App.h` in `src/app_demo/should_be_generated/app/`:
`std::unique_ptr<app::CommandProcessLoopInterface> mCommandProcessLoop;`
`std::shared_ptr<app::EventApplyLoopInterface> mEventApplyLoop;`
and `std::vector<std::shared_ptr<Event>> *events` in handlers.

## The Java developer's map
| Java | C++ | When |
|---|---|---|
| `new Foo()` + GC | `std::make_unique<Foo>()` | **default**: exactly one owner |
| shared references everywhere | `std::make_shared<Foo>()` | genuinely shared ownership (Gringofts: commands passed between threads/loops) |
| `WeakReference<Foo>` | `std::weak_ptr<Foo>` | observe without owning; break cycles |
| a reference you don't own | `Foo &` or `Foo *` (raw) | non-owning, lifetime guaranteed by someone else |

Rules of thumb:
- A raw pointer `T*` in modern code means **"I don't own this"**. Never `delete` it.
- `unique_ptr` is move-only: `consume(std::move(p))` transfers ownership.
- `shared_ptr` uses an atomic ref count: copying it is not free; pass
  `const std::shared_ptr<T>&` or just `T&` when you don't need to share ownership.
- Reference cycles of `shared_ptr` **leak** (no GC to find them). Use `weak_ptr` for back-edges.

## Tasks (`src/smart.h`)
1. `makeWidget(int id)` → `std::unique_ptr<Widget>`.
2. `consume(std::unique_ptr<Widget> w)` — takes ownership (sink), returns id;
   the Widget must be destroyed when `consume` returns.
3. `class TreeNode` — children owned via `std::vector<std::shared_ptr<TreeNode>>`,
   parent observed via `std::weak_ptr<TreeNode>`.
   - `static std::shared_ptr<TreeNode> create(std::string name)`
   - `addChild(const std::shared_ptr<TreeNode> &parent, std::shared_ptr<TreeNode> child)`
     (static; sets child's parent to `parent`)
   - `std::shared_ptr<TreeNode> parent() const` → `mParent.lock()`
   - Test verifies that dropping the root frees the whole tree (no cycle leak).
4. `Registry` stores `std::vector<std::unique_ptr<Widget>>` and hands out
   **non-owning** `Widget *` via `find(id)` (nullptr if missing).

## Experiments
- Change `mParent` to `std::shared_ptr` and watch the "no leak" test fail.
  Then build with `-DJ2C_SANITIZER=address` and read LeakSanitizer's report.
- Try `auto copy = uniquePtr;` and read the "use of deleted function" error.
  You'll see it a lot; now you know what it means.
