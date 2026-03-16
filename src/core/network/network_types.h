#pragma once

#include <string>

namespace lan::network {

struct NetworkInfo {
  std::string mode;      // "lan", "wifi", "hotspot" or "wifi-direct"
  std::string ssid;      // hotspot only
  std::string password;  // hotspot only
  std::string hostIp;    // IPv4 on the created LAN
};

struct NetworkCapabilities {
  bool wifiAdapterPresent = false;
  bool hotspotSupported = false;
  bool processElevated = false;
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

} // namespace lan::network
