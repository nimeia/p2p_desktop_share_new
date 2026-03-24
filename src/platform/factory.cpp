#include "platform/abstraction/factory.h"
#include "platform/abstraction/platform_service_facade.h"

#if defined(_WIN32)
#include "platform/windows/network_service_win.h"
#include "platform/windows/system_actions_win.h"
#elif defined(__APPLE__)
#include "platform/macos/system_actions_macos.h"
#include "platform/posix/network_service_posix.h"
#else
#include "platform/posix/network_service_posix.h"
#include "platform/posix/system_actions_posix.h"
#endif

namespace lan::platform {

std::unique_ptr<INetworkService> CreateDefaultNetworkService() {
#if defined(_WIN32)
  return std::make_unique<lan::platform::windows::WindowsNetworkService>();
#else
  return std::make_unique<lan::platform::posix::PosixNetworkService>();
#endif
}

std::unique_ptr<ISystemActions> CreateDefaultSystemActions() {
#if defined(_WIN32)
  return std::make_unique<lan::platform::windows::WindowsSystemActions>();
#elif defined(__APPLE__)
  return std::make_unique<lan::platform::macos::MacOSSystemActions>();
#else
  return std::make_unique<lan::platform::posix::PosixSystemActions>();
#endif
}

std::unique_ptr<PlatformServiceFacade> CreateDefaultPlatformServiceFacade() {
  return std::make_unique<PlatformServiceFacade>(CreateDefaultSystemActions());
}

} // namespace lan::platform
