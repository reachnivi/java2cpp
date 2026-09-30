# 08 — `std::optional`, `std::string_view`, exceptions & error codes

**Time:** 1.5 h · **Gringofts link:** `INIReader` (~90 uses) for all config —
see `conf/*.ini` and `src/app_util/AppInfo.cpp`; `std::optional<uint64_t> reserved`
in `Command::onPersistFailed`; `std::string_view dedupId()` in `Command.h`;
`ProcessHint{code, message}` in `src/infra/es/ProcessCommandStateMachine.h`.

## Error handling styles you'll meet in Gringofts
1. **Exceptions** — mostly at startup / config (`throw std::runtime_error`), and
   `QueueStoppedException` for shutdown. Catch by `const &`:
   `catch (const std::exception &e) { SPDLOG_ERROR("{}", e.what()); }`
   There are no checked exceptions; nothing forces callers to catch.
2. **Status/result values** on the hot path — `ProcessHint{200, "Success"}`,
   gRPC `Status`, RocksDB `Status`, `bool` + out-param.
3. **`std::optional<T>`** for "maybe no value" (never `nullptr` for values).

## `std::string_view`
A non-owning (pointer, length) view of characters. Cheap to pass, great for
parsing, **dangerous to store**: if the underlying string dies, the view dangles.
Note Gringofts' comment on `dedupId()`: *"if command is alive, the string_view is alive"*.
```cpp
std::string_view bad() { std::string s = "temp"; return s; }  // dangles!
```

## Tasks (`src/config.h`)
1. `parsePort(std::string_view s)` → `std::optional<int>`; valid only if
   all digits and 1..65535.
2. `class Config` parsed from INI text (like the files in Gringofts `conf/`):
   ```ini
   ; comment
   [raft]
   cluster.conf = 1@10.0.0.1:5254,2@10.0.0.2:5254
   storage.type=file
   [grpc]
   port = 50051
   ```
   - `static Config parse(std::string_view text)` — throw `std::invalid_argument`
     on a line that is neither blank, comment (`;` or `#`), `[section]`,
     nor `key = value`. Keys/values are trimmed.
   - `std::optional<std::string> get(section, key) const`
   - `std::string getOr(section, key, fallback) const`
   - `int getInt(section, key) const` — throws `std::out_of_range` if missing,
     `std::invalid_argument` if not an int.
3. `ProcessHint validateIncrease(int current, int requested)` — mirror of
   `IncreaseHandler::process` in Gringofts' demo app: `requested <= current` →
   `{201, "Duplicated request"}`; `requested > current + 1` → `{400, "Invalid request"}`;
   else `{200, "Success"}`.
