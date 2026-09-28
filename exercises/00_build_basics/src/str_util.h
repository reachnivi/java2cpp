#ifndef J2C_STR_UTIL_H_
#define J2C_STR_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

namespace j2c {

// A "static utility class" like Java's StringUtils. In C++ you would often use
// free functions in a namespace instead; Gringofts uses both styles.
class StrUtil {
 public:
  // Declarations only. Definitions live in str_util.cpp.
  static std::vector<std::string> split(const std::string &s, char delim);
  static std::string trim(const std::string &s);
  static std::string join(const std::vector<std::string> &parts, const std::string &sep);

  // Defined in the header. Functions defined inside a class body are
  // implicitly inline, so this one is ODR-safe.
  static bool startsWith(std::string_view s, std::string_view prefix) {
    // TODO: implement (hint: std::string_view::substr / compare)
    (void)s;
    (void)prefix;
    return false;
  }
};

}  // namespace j2c

#endif  // J2C_STR_UTIL_H_
