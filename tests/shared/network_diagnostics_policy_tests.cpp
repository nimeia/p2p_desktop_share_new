#include "core/runtime/network_diagnostics_policy.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "network diagnostics policy test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  RuntimeSessionState session;
  session.hostIp = L"192.168.1.40";
  session.hostPageState = L"sharing";

  RuntimeHealthState healthy;
  healthy.serverProcessRunning = true;
  healthy.portReady = true;
  healthy.lanBindReady = true;
  healthy.hostIpReachable = true;
  healthy.firewallReady = true;
  healthy.firewallDetail = L"Inbound allow rule detected";
  healthy.activeIpv4Candidates = 1;
  healthy.selectedIpRecommended = true;

  const auto healthyModel = BuildNetworkDiagnosticsViewModel(session, healthy);
  Expect(healthyModel.firewallReady, "healthy model should mark firewall ready");
  Expect(healthyModel.remoteViewerReady, "healthy model should mark remote viewer path ready");
  Expect(healthyModel.remoteViewerLabel.find(L"ready") != std::wstring::npos ||
             healthyModel.remoteViewerLabel.find(L"Ready") != std::wstring::npos,
         "healthy model should include ready label");

  RuntimeHealthState firewallBlocked = healthy;
  firewallBlocked.firewallReady = false;
  firewallBlocked.firewallDetail = L"No inbound allow rule detected";
  const auto blockedModel = BuildNetworkDiagnosticsViewModel(session, firewallBlocked);
  Expect(!blockedModel.firewallReady, "blocked model should not mark firewall ready");
  Expect(!blockedModel.remoteViewerReady, "blocked model should not mark remote path ready");
  Expect(blockedModel.remoteViewerDetail.find(L"Firewall") != std::wstring::npos ||
             blockedModel.remoteViewerDetail.find(L"firewall") != std::wstring::npos,
         "blocked model should mention firewall in detail");

  RuntimeHealthState adapterMismatch = healthy;
  adapterMismatch.activeIpv4Candidates = 2;
  adapterMismatch.selectedIpRecommended = false;
  adapterMismatch.adapterHint = L"Ethernet looks more stable than Wi-Fi for this room.";
  const auto adapterModel = BuildNetworkDiagnosticsViewModel(session, adapterMismatch);
  Expect(!adapterModel.remoteViewerReady, "adapter mismatch should block remote path");
  Expect(adapterModel.remoteViewerLabel.find(L"Adapter") != std::wstring::npos,
         "adapter mismatch should surface adapter label");

  RuntimeHealthState stopped;
  const auto stoppedModel = BuildNetworkDiagnosticsViewModel(session, stopped);
  Expect(!stoppedModel.firewallReady, "stopped server should not mark firewall ready");
  Expect(stoppedModel.firewallLabel == L"Deferred", "stopped server should defer firewall check");
  Expect(stoppedModel.remoteViewerLabel == L"Server stopped", "stopped server should block remote path");

  std::cout << "network diagnostics policy tests passed\n";
  return 0;
}
