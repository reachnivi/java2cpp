#ifndef J2C_STR_UTIL_H_
#define J2C_STR_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

namespace j2c {

class StrUtil {
 public:
  static std::vector<std::string> split(const std::string &s, char delim);
  static std::string trim(const std::string &s);
  static std::string join(const std::vector<std::string> &parts, const std::string &sep);

  static bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
  }
};

}  // namespace j2c

#endif  // J2C_STR_UTIL_H_
