#pragma once

#include "core/network/network_types.h"

#include <string>
#include <vector>

namespace lan::runtime {

struct HostRuntimeRefreshInput {
  std::wstring fallbackHostIp;
  std::wstring existingNetworkMode;
  std::wstring existingHotspotSsid;
  std::wstring existingHotspotPassword;
  bool existingWifiAdapterPresent = false;
  bool existingHotspotSupported = false;
  bool existingWifiDirectApiAvailable = false;

  bool networkInfoAvailable = false;
  lan::network::NetworkInfo networkInfo;
  std::string networkInfoError;

  bool capabilitiesAvailable = false;
  lan::network::NetworkCapabilities capabilities;
  std::string capabilitiesError;

  bool hotspotStateAvailable = false;
  lan::network::HotspotState hotspotState;
  std::string hotspotStateError;
};

struct HostRuntimeRefreshResult {
  std::wstring hostIp;
  std::wstring networkMode;
  std::wstring hotspotSsid;
  std::wstring hotspotPassword;
  std::wstring hotspotStatus;
  bool hotspotRunning = false;
  bool wifiAdapterPresent = false;
  bool hotspotSupported = false;
  bool wifiDirectApiAvailable = false;
  std::vector<std::wstring> logLines;
};

HostRuntimeRefreshResult CoordinateHostRuntimeRefresh(const HostRuntimeRefreshInput& input);
std::wstring BuildNetworkCapabilitiesText(const HostRuntimeRefreshResult& result);

} // namespace lan::runtime
