#pragma once
#include <string>

namespace lan::util {
  void LogInfo(const std::string& msg);
  void LogWarn(const std::string& msg);
  void LogError(const std::string& msg);
}
