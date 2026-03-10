#include "log.h"
#include <iostream>

namespace lan::util {
void LogInfo(const std::string& msg) { std::cout << "[I] " << msg << std::endl; }
void LogWarn(const std::string& msg) { std::cout << "[W] " << msg << std::endl; }
void LogError(const std::string& msg) { std::cerr << "[E] " << msg << std::endl; }
}
