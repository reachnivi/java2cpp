#ifndef J2C_CONFIG_H_
#define J2C_CONFIG_H_

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace j2c {

inline std::optional<int> parsePort(std::string_view s) {
  // TODO: reject empty / non-digit / out of range. Beware overflow on long
  // inputs: check length or accumulate in a wider type (int64_t) with an early exit.
  (void)s;
  return std::nullopt;
}

class Config {
 public:
  static Config parse(std::string_view text) {
    // TODO: split text into lines, trim each, skip blanks and comments,
    // track the current section, store key/value pairs.
    // Throw std::invalid_argument("line N: ...") on malformed input.
    (void)text;
    return Config();
  }

  std::optional<std::string> get(const std::string &section, const std::string &key) const {
    // TODO: use mValues.find(...) — NOT operator[] (it's non-const and inserts).
    (void)section;
    (void)key;
    return std::nullopt;
  }

  std::string getOr(const std::string &section, const std::string &key,
                    const std::string &fallback) const {
    // TODO: one line with std::optional::value_or
    (void)section;
    (void)key;
    return fallback + "TODO";
  }

  int getInt(const std::string &section, const std::string &key) const {
    // TODO: std::out_of_range if missing; std::invalid_argument if not an int
    (void)section;
    (void)key;
    return 0;
  }

 private:
  // key is (section, key). std::pair has operator< so it works as a map key.
  std::map<std::pair<std::string, std::string>, std::string> mValues;
};

// Same shape as gringofts::ProcessHint
struct ProcessHint {
  uint32_t mCode;
  std::string mMessage;
};

inline ProcessHint validateIncrease(int current, int requested) {
  // TODO
  (void)current;
  (void)requested;
  return ProcessHint{0, ""};
}

}  // namespace j2c

#endif  // J2C_CONFIG_H_
