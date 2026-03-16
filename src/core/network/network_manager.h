#pragma once

#include "core/network/network_types.h"

#include <string>

namespace lan::network {

class NetworkManager {
public:
  static bool GetCurrentNetworkInfo(NetworkInfo& out, std::string& err);
  static bool QueryCapabilities(NetworkCapabilities& out, std::string& err);
  static HotspotConfig MakeSuggestedHotspotConfig();
  static bool StartHotspot(const HotspotConfig& cfg, HotspotState& out, std::string& err);
  static bool StopHotspot(std::string& err);
  static bool QueryHotspotState(HotspotState& out, std::string& err);
};

} // namespace lan::network
