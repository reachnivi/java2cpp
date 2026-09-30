#ifndef J2C_CONFIG_H_
#define J2C_CONFIG_H_

#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace j2c {

inline std::string_view trimView(std::string_view s) {
  const char *ws = " \t\r\n";
  auto b = s.find_first_not_of(ws);
  if (b == std::string_view::npos) {
    return {};
  }
  auto e = s.find_last_not_of(ws);
  return s.substr(b, e - b + 1);
}

inline std::optional<int> parsePort(std::string_view s) {
  if (s.empty() || s.size() > 5) {
    return std::nullopt;
  }
  int value = 0;
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return std::nullopt;
    }
    value = value * 10 + (c - '0');
  }
  if (value < 1 || value > 65535) {
    return std::nullopt;
  }
  return value;
}

class Config {
 public:
  static Config parse(std::string_view text) {
    Config cfg;
    std::string section;
    int lineNo = 0;
    while (!text.empty()) {
      auto nl = text.find('\n');
      std::string_view line = text.substr(0, nl);
      text = (nl == std::string_view::npos) ? std::string_view{} : text.substr(nl + 1);
      ++lineNo;

      line = trimView(line);
      if (line.empty() || line.front() == ';' || line.front() == '#') {
        continue;
      }
      if (line.front() == '[') {
        if (line.back() != ']') {
          throw std::invalid_argument("line " + std::to_string(lineNo) + ": unterminated section");
        }
        section = std::string(trimView(line.substr(1, line.size() - 2)));
        continue;
      }
      auto eq = line.find('=');
      if (eq == std::string_view::npos) {
        throw std::invalid_argument("line " + std::to_string(lineNo) + ": expected key = value");
      }
      std::string key(trimView(line.substr(0, eq)));
      std::string value(trimView(line.substr(eq + 1)));
      cfg.mValues[{section, key}] = value;
    }
    return cfg;
  }

  std::optional<std::string> get(const std::string &section, const std::string &key) const {
    auto it = mValues.find({section, key});
    if (it == mValues.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  std::string getOr(const std::string &section, const std::string &key,
                    const std::string &fallback) const {
    return get(section, key).value_or(fallback);
  }

  int getInt(const std::string &section, const std::string &key) const {
    auto v = get(section, key);
    if (!v) {
      throw std::out_of_range("missing config " + section + "." + key);
    }
    std::size_t consumed = 0;
    int result = std::stoi(*v, &consumed);  // throws std::invalid_argument
    if (consumed != v->size()) {
      throw std::invalid_argument("not an int: " + *v);
    }
    return result;
  }

 private:
  std::map<std::pair<std::string, std::string>, std::string> mValues;
};

struct ProcessHint {
  uint32_t mCode;
  std::string mMessage;
};

inline ProcessHint validateIncrease(int current, int requested) {
  if (requested <= current) {
    return {201, "Duplicated request"};
  }
  if (requested > current + 1) {
    return {400, "Invalid request"};
  }
  return {200, "Success"};
}

}  // namespace j2c

#endif  // J2C_CONFIG_H_
