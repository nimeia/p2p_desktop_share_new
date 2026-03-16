#include "platform/abstraction/platform_service_facade.h"

#include "core/network/network_manager.h"
#include "platform/abstraction/system_actions.h"

namespace lan::platform {

PlatformServiceFacade::PlatformServiceFacade(std::unique_ptr<ISystemActions> systemActions)
    : systemActions_(std::move(systemActions)) {}

PlatformServiceFacade::~PlatformServiceFacade() = default;

const char* PlatformServiceFacade::SystemProviderName() const {
  return systemActions_ ? systemActions_->ProviderName() : "missing";
}

bool PlatformServiceFacade::GetCurrentNetworkInfo(lan::network::NetworkInfo& out, std::string& err) const {
  return lan::network::NetworkManager::GetCurrentNetworkInfo(out, err);
}

bool PlatformServiceFacade::QueryNetworkCapabilities(lan::network::NetworkCapabilities& out, std::string& err) const {
  return lan::network::NetworkManager::QueryCapabilities(out, err);
}

lan::network::HotspotConfig PlatformServiceFacade::MakeSuggestedHotspotConfig() const {
  return lan::network::NetworkManager::MakeSuggestedHotspotConfig();
}

bool PlatformServiceFacade::StartHotspot(const lan::network::HotspotConfig& cfg,
                                         lan::network::HotspotState& out,
                                         std::string& err) const {
  return lan::network::NetworkManager::StartHotspot(cfg, out, err);
}

bool PlatformServiceFacade::StopHotspot(std::string& err) const {
  return lan::network::NetworkManager::StopHotspot(err);
}

bool PlatformServiceFacade::QueryHotspotState(lan::network::HotspotState& out, std::string& err) const {
  return lan::network::NetworkManager::QueryHotspotState(out, err);
}

bool PlatformServiceFacade::OpenWifiDirectPairing(std::string& err) {
  if (!systemActions_) {
    err = "System actions provider is not available.";
    return false;
  }
  return systemActions_->OpenSystemPage(SystemPage::ConnectedDevices, err);
}

bool PlatformServiceFacade::OpenSystemHotspotSettings(std::string& err) {
  if (!systemActions_) {
    err = "System actions provider is not available.";
    return false;
  }
  return systemActions_->OpenSystemPage(SystemPage::MobileHotspot, err);
}

bool PlatformServiceFacade::OpenFirewallSettings(std::string& err) {
  if (!systemActions_) {
    err = "System actions provider is not available.";
    return false;
  }
  return systemActions_->OpenSystemPage(SystemPage::Firewall, err);
}

bool PlatformServiceFacade::OpenExternalUrl(std::string_view url, std::string& err) {
  if (!systemActions_) {
    err = "System actions provider is not available.";
    return false;
  }
  return systemActions_->OpenExternalUrl(url, err);
}

bool PlatformServiceFacade::OpenExternalPath(const std::filesystem::path& path, std::string& err) {
  if (!systemActions_) {
    err = "System actions provider is not available.";
    return false;
  }
  return systemActions_->OpenExternalPath(path, err);
}


bool PlatformServiceFacade::ShowNotification(std::string_view title, std::string_view body, std::string& err) {
  if (!systemActions_) {
    err = "System actions provider is not available.";
    return false;
  }
  return systemActions_->ShowNotification(title, body, err);
}

} // namespace lan::platform
