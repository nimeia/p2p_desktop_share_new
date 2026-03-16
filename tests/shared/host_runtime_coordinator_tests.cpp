#include "core/runtime/host_runtime_coordinator.h"

#include <iostream>

namespace {

int Check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << "\n";
    return 1;
  }
  return 0;
}

} // namespace

int main() {
  using namespace lan::runtime;

  HostRuntimeRefreshInput input;
  input.fallbackHostIp = L"192.168.1.10";
  input.existingHotspotSsid = L"KeepMe";
  input.existingHotspotPassword = L"Secret";
  input.networkInfoError = "provider missing";
  input.capabilitiesAvailable = true;
  input.capabilities.wifiAdapterPresent = true;
  input.capabilities.hotspotSupported = true;
  input.capabilities.wifiDirectApiAvailable = false;
  input.hotspotStateAvailable = true;
  input.hotspotState.supported = true;
  input.hotspotState.running = true;
  input.hotspotState.hostIp = "172.20.10.1";
  input.hotspotState.ssid = "DeskShare";

  auto result = CoordinateHostRuntimeRefresh(input);
  if (int rc = Check(result.hostIp == L"172.20.10.1", "hotspot host IP should override fallback")) return rc;
  if (int rc = Check(result.hotspotRunning, "hotspot should be running")) return rc;
  if (int rc = Check(result.hotspotStatus == L"running", "hotspot status should be running")) return rc;
  if (int rc = Check(result.hotspotSsid == L"DeskShare", "hotspot ssid should come from probe")) return rc;
  if (int rc = Check(result.hotspotPassword == L"Secret", "existing password should be preserved when probe omits it")) return rc;
  if (int rc = Check(result.wifiAdapterPresent, "capabilities should propagate")) return rc;
  if (int rc = Check(result.hotspotSupported, "hotspot support should propagate")) return rc;
  if (int rc = Check(!result.logLines.empty(), "fallback path should emit logs")) return rc;

  HostRuntimeRefreshInput direct;
  direct.fallbackHostIp = L"127.0.0.1";
  direct.networkInfoAvailable = true;
  direct.networkInfo.mode = "wifi";
  direct.networkInfo.hostIp = "10.0.0.8";
  auto directResult = CoordinateHostRuntimeRefresh(direct);
  if (int rc = Check(directResult.hostIp == L"10.0.0.8", "network info host IP should win when available")) return rc;
  if (int rc = Check(directResult.networkMode == L"wifi", "network mode should come from network info")) return rc;
  if (int rc = Check(directResult.logLines.front().find(L"Network detected") != std::wstring::npos,
                     "network success should log detection")) return rc;

  std::wcout << L"host runtime coordinator tests passed\n";
  return 0;
}
