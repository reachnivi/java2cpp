#include "str_util.h"

namespace j2c {

std::vector<std::string> StrUtil::split(const std::string &s, char delim) {
  // TODO: implement. Hint: std::string::find(delim, pos) returns
  // std::string::npos when not found; std::string::substr(pos, len).
  (void)s;
  (void)delim;
  return {};
}

std::string StrUtil::trim(const std::string &s) {
  // TODO: implement. Hint: find_first_not_of(" \t\n\r") / find_last_not_of.
  return s;
}

std::string StrUtil::join(const std::vector<std::string> &parts, const std::string &sep) {
  // TODO: implement. Use a range-for: for (const auto &p : parts) { ... }
  (void)parts;
  (void)sep;
  return "";
}

}  // namespace j2c
