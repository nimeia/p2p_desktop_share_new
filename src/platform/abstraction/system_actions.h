#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace lan::platform {

enum class SystemPage {
  ConnectedDevices,
  MobileHotspot,
  Firewall,
};

class ISystemActions {
public:
  virtual ~ISystemActions() = default;

  virtual const char* ProviderName() const = 0;
  virtual bool OpenSystemPage(SystemPage page, std::string& err) = 0;
  virtual bool OpenExternalUrl(std::string_view url, std::string& err) = 0;
  virtual bool OpenExternalPath(const std::filesystem::path& path, std::string& err) = 0;
  virtual bool ShowNotification(std::string_view title, std::string_view body, std::string& err) = 0;
};

} // namespace lan::platform
