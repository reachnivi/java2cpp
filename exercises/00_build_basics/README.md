# 00 — Build basics: headers, sources, namespaces, linking

**Time:** 45 min · **Gringofts link:** `src/infra/util/StrUtil.h`, `src/infra/util/FileUtil.{h,cpp}`

## Why
Java has one file per class and the JVM finds classes at runtime. C++ has
**translation units**: each `.cpp` is compiled on its own, with its `#include`d
headers pasted in by the preprocessor. The **linker** then stitches object files
together. Almost every "weird" C++ build error is one of these:

| Error | Meaning | Java-ish analogy |
|---|---|---|
| `'foo' was not declared in this scope` | Compiler has not seen a declaration yet (missing `#include`) | missing `import` |
| `undefined reference to 'foo'` | Declared, but no `.cpp` defining it was linked | class on compile path but not runtime classpath |
| `multiple definition of 'foo'` | Defined in a header that is included in 2+ `.cpp` files (ODR violation) | — |

## Read first
- Header guards: Gringofts uses `#ifndef SRC_INFRA_UTIL_STRUTIL_H_` style
  (cpplint enforces it). `#pragma once` is the modern shortcut.
- `namespace gringofts { ... }` ≈ Java `package`, but purely a naming scope.
- `const std::string &s` — "pass by const reference": no copy, can't modify.
  This is the default way to pass non-trivial objects in Gringofts.

## Tasks
Edit `src/str_util.h` (declarations) and `src/str_util.cpp` (definitions):

1. `split(const std::string &s, char delim)` → `std::vector<std::string>`
   (`"a,b,,c"` → `{"a","b","","c"}`).
2. `trim(const std::string &s)` → copy without leading/trailing whitespace.
3. `join(const std::vector<std::string> &parts, const std::string &sep)`.
4. `startsWith(std::string_view s, std::string_view prefix)` — already defined
   **in the header**, inside the class body (implicitly `inline`).

## Experiments (do them, they take 2 minutes each)
- Add a free function `bool isBlank(const std::string &s) { ... }` to the
  header *outside* the class, without `inline`. Add a second file
  `src/other.cpp` that just does `#include "str_util.h"`. Re-run cmake and build:
  read the "multiple definition" error. Fix it with `inline`.
- Delete the definition of `trim` from the `.cpp`. Read the linker error.
- Remove `#include <vector>` from the header. Does it still compile? Why might
  it (transitive include), and why is relying on that fragile?
- Run `g++ -std=c++17 -E src/str_util.cpp | wc -l` to see how big the
  preprocessed translation unit is.
