#include "core/network/network_manager.h"

#include "core/network/endpoint_selection.h"

#if defined(_WIN32)
#include "platform/windows/network_actions_win.h"
#include "platform/windows/network_probe_win.h"
#else
#include "platform/posix/network_actions_posix.h"
#include "platform/posix/network_probe_posix.h"
#endif

namespace lan::network {

bool NetworkManager::GetCurrentNetworkInfo(NetworkInfo& out, std::string& err) {
  out = {};
  EndpointProbeResult probe;
#if defined(_WIN32)
  lan::platform::windows::ProbeNetworkInterfaces(probe, err);
#else
  lan::platform::posix::ProbeNetworkInterfaces(probe, err);
#endif
  std::string detail;
  if (SelectPreferredNetworkInfo(probe, out, detail)) {
    err.clear();
    return true;
  }
  err = detail.empty() ? err : detail;
  return false;
}

bool NetworkManager::QueryCapabilities(NetworkCapabilities& out, std::string& err) {
#if defined(_WIN32)
  return lan::platform::windows::QueryCapabilities(out, err);
#else
  return lan::platform::posix::QueryCapabilities(out, err);
#endif
}

HotspotConfig NetworkManager::MakeSuggestedHotspotConfig() {
#if defined(_WIN32)
  return lan::platform::windows::MakeSuggestedHotspotConfig();
#else
  return lan::platform::posix::MakeSuggestedHotspotConfig();
#endif
}

bool NetworkManager::StartHotspot(const HotspotConfig& cfg, HotspotState& out, std::string& err) {
#if defined(_WIN32)
  return lan::platform::windows::StartHotspot(cfg, out, err);
#else
  return lan::platform::posix::StartHotspot(cfg, out, err);
#endif
}

bool NetworkManager::StopHotspot(std::string& err) {
#if defined(_WIN32)
  return lan::platform::windows::StopHotspot(err);
#else
  return lan::platform::posix::StopHotspot(err);
#endif
}

bool NetworkManager::QueryHotspotState(HotspotState& out, std::string& err) {
#if defined(_WIN32)
  return lan::platform::windows::QueryHotspotState(out, err);
#else
  return lan::platform::posix::QueryHotspotState(out, err);
#endif
}

} // namespace lan::network
