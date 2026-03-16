#include "core/runtime/host_runtime_coordinator.h"

#include <string_view>

namespace lan::runtime {
namespace {

std::wstring AsciiWide(std::string_view text) {
  return std::wstring(text.begin(), text.end());
}

std::wstring DefaultNetworkMode(std::wstring_view hostIp) {
  return hostIp.empty() ? L"unknown" : L"lan";
}

std::wstring ResolveHotspotStatus(const lan::network::HotspotState& state) {
  if (state.running) return L"running";
  if (!state.supported) return L"system settings required";
  return L"stopped";
}

} // namespace

HostRuntimeRefreshResult CoordinateHostRuntimeRefresh(const HostRuntimeRefreshInput& input) {
  HostRuntimeRefreshResult result;
  result.hostIp = input.fallbackHostIp;
  result.networkMode = input.existingNetworkMode.empty() ? DefaultNetworkMode(result.hostIp) : input.existingNetworkMode;
  result.hotspotSsid = input.existingHotspotSsid;
  result.hotspotPassword = input.existingHotspotPassword;
  result.hotspotStatus = L"stopped";
  result.hotspotRunning = false;
  result.wifiAdapterPresent = input.existingWifiAdapterPresent;
  result.hotspotSupported = input.existingHotspotSupported;
  result.wifiDirectApiAvailable = input.existingWifiDirectApiAvailable;

  if (input.networkInfoAvailable && !input.networkInfo.hostIp.empty()) {
    result.hostIp = AsciiWide(input.networkInfo.hostIp);
    result.networkMode = input.networkInfo.mode.empty() ? L"lan" : AsciiWide(input.networkInfo.mode);
    result.logLines.push_back(L"Network detected: mode=" + result.networkMode + L", ip=" + result.hostIp);
  } else {
    result.networkMode = DefaultNetworkMode(result.hostIp);
    if (!input.networkInfoError.empty()) {
      result.logLines.push_back(L"NetworkManager fallback: " + AsciiWide(input.networkInfoError));
    }
    result.logLines.push_back(L"Host IP refreshed: " + (result.hostIp.empty() ? std::wstring(L"(none)") : result.hostIp));
  }

  if (input.capabilitiesAvailable) {
    result.wifiAdapterPresent = input.capabilities.wifiAdapterPresent;
    result.hotspotSupported = input.capabilities.hotspotSupported;
    result.wifiDirectApiAvailable = input.capabilities.wifiDirectApiAvailable;
  } else if (!input.capabilitiesError.empty()) {
    result.logLines.push_back(L"Capability probe failed: " + AsciiWide(input.capabilitiesError));
  }

  if (input.hotspotStateAvailable) {
    result.hotspotRunning = input.hotspotState.running;
    result.hotspotStatus = ResolveHotspotStatus(input.hotspotState);
    if (!input.hotspotState.ssid.empty()) result.hotspotSsid = AsciiWide(input.hotspotState.ssid);
    if (!input.hotspotState.password.empty()) result.hotspotPassword = AsciiWide(input.hotspotState.password);
    if (!input.hotspotState.hostIp.empty()) {
      result.hostIp = AsciiWide(input.hotspotState.hostIp);
    }
  } else if (!input.hotspotStateError.empty()) {
    result.logLines.push_back(L"Hotspot state probe: " + AsciiWide(input.hotspotStateError));
  }

  return result;
}

std::wstring BuildNetworkCapabilitiesText(const HostRuntimeRefreshResult& result) {
  std::wstring text;
  text += L"Wi-Fi adapter: ";
  text += result.wifiAdapterPresent ? L"present" : L"missing";
  text += L"\r\nHotspot control: ";
  text += result.hotspotSupported ? L"supported / best effort" : L"not detected; use Windows Mobile Hotspot settings";
  text += L"\r\nWi-Fi Direct API: ";
  text += result.wifiDirectApiAvailable ? L"available (pair via Windows UI)" : L"not detected";
  return text;
}

} // namespace lan::runtime
