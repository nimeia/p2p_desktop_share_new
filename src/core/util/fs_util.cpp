#include "fs_util.h"
#include <fstream>
#include <sstream>

namespace lan::util {
std::string ReadAllText(const std::string& path) {
  std::ifstream ifs(path, std::ios::binary);
  std::ostringstream oss;
  oss << ifs.rdbuf();
  return oss.str();
}
}
