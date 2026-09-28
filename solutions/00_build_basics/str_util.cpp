#include "str_util.h"

namespace j2c {

std::vector<std::string> StrUtil::split(const std::string &s, char delim) {
  std::vector<std::string> out;
  std::string::size_type start = 0;
  while (true) {
    auto pos = s.find(delim, start);
    if (pos == std::string::npos) {
      out.push_back(s.substr(start));
      return out;
    }
    out.push_back(s.substr(start, pos - start));
    start = pos + 1;
  }
}

std::string StrUtil::trim(const std::string &s) {
  const char *ws = " \t\n\r\f\v";
  auto begin = s.find_first_not_of(ws);
  if (begin == std::string::npos) {
    return "";
  }
  auto end = s.find_last_not_of(ws);
  return s.substr(begin, end - begin + 1);
}

std::string StrUtil::join(const std::vector<std::string> &parts, const std::string &sep) {
  std::string out;
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i > 0) {
      out += sep;
    }
    out += parts[i];
  }
  return out;
}

}  // namespace j2c
