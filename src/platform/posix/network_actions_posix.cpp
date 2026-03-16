#include "platform/posix/network_actions_posix.h"

namespace lan::platform::posix {

bool QueryCapabilities(lan::network::NetworkCapabilities& out, std::string& err) {
  out = {};
  err = "Network capabilities are not implemented on POSIX in this MVP.";
  return false;
}

lan::network::HotspotConfig MakeSuggestedHotspotConfig() {
  return {"LanShare", "LanShare123"};
}

bool StartHotspot(const lan::network::HotspotConfig&, lan::network::HotspotState& out, std::string& err) {
  out = {};
  err = "Hotspot control is not implemented on POSIX in this MVP.";
  return false;
}

bool StopHotspot(std::string& err) {
  err = "Hotspot control is not implemented on POSIX in this MVP.";
  return false;
}

bool QueryHotspotState(lan::network::HotspotState& out, std::string& err) {
  out = {};
  err = "Hotspot control is not implemented on POSIX in this MVP.";
  return false;
}

} // namespace lan::platform::posix
