#include "platform/abstraction/runtime_paths.h"

namespace lan::platform {

std::filesystem::path ExecutableDir(const char* argv0) {
  try {
    return std::filesystem::weakly_canonical(std::filesystem::path(argv0)).parent_path();
  } catch (...) {
    return std::filesystem::current_path();
  }
}

std::string ResolveRuntimePath(const std::filesystem::path& baseDir, const std::string& value) {
  std::filesystem::path path(value);
  if (path.is_absolute()) return path.string();
  return (baseDir / path).lexically_normal().string();
}

} // namespace lan::platform
