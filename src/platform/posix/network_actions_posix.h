#pragma once

#include "core/network/network_types.h"

#include <string>

namespace lan::platform::posix {

bool QueryCapabilities(lan::network::NetworkCapabilities& out, std::string& err);
lan::network::HotspotConfig MakeSuggestedHotspotConfig();
bool StartHotspot(const lan::network::HotspotConfig& cfg, lan::network::HotspotState& out, std::string& err);
bool StopHotspot(std::string& err);
bool QueryHotspotState(lan::network::HotspotState& out, std::string& err);

} // namespace lan::platform::posix
