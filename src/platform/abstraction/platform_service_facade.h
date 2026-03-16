#pragma once

#include "core/network/network_types.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace lan::platform {

class ISystemActions;

class PlatformServiceFacade {
public:
  explicit PlatformServiceFacade(std::unique_ptr<ISystemActions> systemActions);
  ~PlatformServiceFacade();

  PlatformServiceFacade(const PlatformServiceFacade&) = delete;
  PlatformServiceFacade& operator=(const PlatformServiceFacade&) = delete;

  const char* SystemProviderName() const;

  bool GetCurrentNetworkInfo(lan::network::NetworkInfo& out, std::string& err) const;
  bool QueryNetworkCapabilities(lan::network::NetworkCapabilities& out, std::string& err) const;
  lan::network::HotspotConfig MakeSuggestedHotspotConfig() const;
  bool StartHotspot(const lan::network::HotspotConfig& cfg,
                    lan::network::HotspotState& out,
                    std::string& err) const;
  bool StopHotspot(std::string& err) const;
  bool QueryHotspotState(lan::network::HotspotState& out, std::string& err) const;

  bool OpenWifiDirectPairing(std::string& err);
  bool OpenSystemHotspotSettings(std::string& err);
  bool OpenFirewallSettings(std::string& err);
  bool OpenExternalUrl(std::string_view url, std::string& err);
  bool OpenExternalPath(const std::filesystem::path& path, std::string& err);
  bool ShowNotification(std::string_view title, std::string_view body, std::string& err);

private:
  std::unique_ptr<ISystemActions> systemActions_;
};

std::unique_ptr<PlatformServiceFacade> CreateDefaultPlatformServiceFacade();

} // namespace lan::platform
