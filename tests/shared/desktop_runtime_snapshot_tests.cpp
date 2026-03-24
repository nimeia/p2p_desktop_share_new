#include "core/runtime/desktop_runtime_snapshot.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "desktop runtime snapshot test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

namespace lan::runtime::tests {

void RunDesktopRuntimeSnapshotTests() {
  DesktopRuntimeSnapshotInput input;
  input.networkMode = L"lan";
  input.hostIp = L"192.168.1.20";
  input.bindAddress = L"0.0.0.0";
  input.port = 9443;
  input.room = L"roomA";
  input.token = L"tokenA";
  input.hostPageState = L"ready";
  input.hotspotStatus = L"stopped";
  input.hotspotSsid = L"LabHotspot";
  input.hotspotPassword = L"password123";
  input.wifiDirectAlias = L"LanShare-roomA";
  input.webviewStatusText = L"ready";
  input.wifiDirectApiAvailable = true;
  input.wifiAdapterPresent = true;
  input.hotspotSupported = true;
  input.viewerUrlCopied = true;
  input.shareCardExported = true;
  input.shareWizardOpened = true;
  input.handoffStarted = true;
  input.lastRooms = 1;
  input.lastViewers = 0;

  input.serverProcessRunning = true;
  input.portReady = true;
  input.portDetail = L"listening";
  input.localHealthReady = true;
  input.localHealthDetail = L"ok";
  input.hostIpReachable = true;
  input.hostIpReachableDetail = L"reachable";
  input.lanBindReady = true;
  input.lanBindDetail = L"bound";
  input.activeIpv4Candidates = 2;
  input.selectedIpRecommended = true;
  input.adapterHint = L"Wi-Fi";
  input.embeddedHostReady = true;
  input.embeddedHostStatus = L"ready";
  input.firewallReady = true;
  input.firewallDetail = L"Inbound allow rule detected";
  input.liveReady = true;

  const auto snapshot = BuildDesktopRuntimeSnapshot(input);
  Expect(snapshot.session.room == L"roomA", "session state should be populated");
  Expect(snapshot.health.portReady, "health state should be populated");
  Expect(snapshot.hostUrl == L"http://127.0.0.1:9443/host?room=roomA&token=tokenA&lang=en", "host url should be derived from session");
  Expect(snapshot.viewerUrl == L"http://192.168.1.20:9443/view?room=roomA&token=tokenA&lang=en", "viewer url should be derived from session");
  Expect(snapshot.dashboardOverall == L"Ready", "healthy ready state should compute Ready dashboard state");
  Expect(snapshot.handoff.state == L"ready-for-handoff", "healthy started handoff should be ready-for-handoff");
  Expect(snapshot.selfCheckSummary.p0 == 0, "healthy snapshot should have no p0 failures");
  Expect(snapshot.selfCheckSummary.summaryLine.find(L"ok") != std::wstring::npos, "summary should mention ok checks");
  Expect(snapshot.recentHeartbeat == L"/health ok", "recent heartbeat should normalize successful health");
  Expect(snapshot.localReachability == L"ok", "local reachability should normalize reachable health");
}

} // namespace lan::runtime::tests
