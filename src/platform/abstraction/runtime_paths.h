#pragma once

#include <filesystem>
#include <string>

namespace lan::platform {

std::filesystem::path ExecutableDir(const char* argv0);
std::string ResolveRuntimePath(const std::filesystem::path& baseDir, const std::string& value);

} // namespace lan::platform
