#pragma once
#include <string>

namespace lan::network {

struct NetworkInfo {
  std::string mode;    // "lan", "wifi", "hotspot" or "wifi-direct"
  std::string ssid;    // hotspot only
  std::string password;// hotspot only
  std::string hostIp;  // IPv4 on the created LAN
};

struct NetworkCapabilities {
  bool wifiAdapterPresent = false;
  bool hotspotSupported = false;
  bool wifiDirectApiAvailable = false;
  bool wifiDirectNeedsPairingUi = false;
  std::string summary;
};

struct HotspotConfig {
  std::string ssid;
  std::string password;
};

struct HotspotState {
  bool supported = false;
  bool running = false;
  std::string mode;
  std::string ssid;
  std::string password;
  std::string hostIp;
  std::string rawStatus;
};

class NetworkManager {
public:
  // MVP: detect the best active LAN adapter and host IPv4.
  static bool GetCurrentNetworkInfo(NetworkInfo& out, std::string& err);

  // Capability probe for Windows desktop LAN/Wi-Fi sharing features.
  static bool QueryCapabilities(NetworkCapabilities& out, std::string& err);

  // Best-effort hotspot control for Windows desktop using hosted-network commands.
  static HotspotConfig MakeSuggestedHotspotConfig();
  static bool StartHotspot(const HotspotConfig& cfg, HotspotState& out, std::string& err);
  static bool StopHotspot(std::string& err);
  static bool QueryHotspotState(HotspotState& out, std::string& err);
};

} // namespace lan::network
