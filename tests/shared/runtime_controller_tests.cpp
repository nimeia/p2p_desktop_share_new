#include "core/runtime/runtime_controller.h"
#include "core/runtime/desktop_runtime_snapshot.h"

#include <cstdlib>
#include <iostream>

namespace lan::runtime::tests {
void RunDesktopRuntimeSnapshotTests();
}

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "runtime controller test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  RuntimeSessionState session;
  session.port = 9443;
  session.room = L"abc123";
  session.token = L"token123";
  session.networkMode = L"lan";
  session.hostPageState = L"ready";
  session.hotspotStatus = L"stopped";
  session.wifiDirectAlias = L"LanScreenShare-abc123";
  session.lastRooms = 1;
  session.lastViewers = 0;

  Expect(BuildHostUrl(session) == L"http://127.0.0.1:9443/host?room=abc123&token=token123&lang=en",
         "host url should fall back to loopback");
  Expect(BuildViewerUrl(session) == L"http://127.0.0.1:9443/view?room=abc123&token=token123&lang=en",
         "viewer url should fall back to loopback");

  RuntimeHealthState health;
  health.serverProcessRunning = true;
  health.portReady = true;
  health.localHealthReady = true;
  health.hostIpReachable = true;
  health.lanBindReady = true;
  health.embeddedHostReady = true;
  health.portDetail = L"listening";
  health.localHealthDetail = L"ok";
  health.hostIpReachableDetail = L"reachable";
  health.lanBindDetail = L"bound";
  health.adapterHint = L"Wi-Fi";
  health.embeddedHostStatus = L"ready";

  auto handoff = BuildHandoffSummary(session, health);
  Expect(handoff.state == L"not-started", "handoff should start as not-started");

  session.handoffStarted = true;
  handoff = BuildHandoffSummary(session, health);
  Expect(handoff.state == L"ready-for-handoff", "healthy in-progress handoff should be ready");

  health.hostIpReachable = false;
  handoff = BuildHandoffSummary(session, health);
  Expect(handoff.state == L"needs-fix", "broken reachability should block handoff");

  session.handoffDelivered = true;
  handoff = BuildHandoffSummary(session, health);
  Expect(handoff.state == L"delivered", "connected viewer should mark delivered");

  health.hostIpReachable = true;
  RuntimeSelfCheckSummary selfCheck;
  selfCheck.summaryLine = L"2 checks need attention";
  selfCheck.networkCount = 2;
  selfCheck.sharingCount = 3;

  const std::wstring shareInfo = BuildShareInfoText(session, health, selfCheck);
  Expect(shareInfo.find(L"Viewer URL") != std::wstring::npos, "share info should include viewer url section");
  Expect(shareInfo.find(L"LanScreenShare-abc123") != std::wstring::npos, "share info should include Wi-Fi Direct alias");
  Expect(shareInfo.find(L"2 checks need attention") != std::wstring::npos, "share info should include self-check summary");

  session.hostPageState = L"sharing";
  Expect(ComputeDashboardOverallState(session, health, 0) == L"Sharing",
         "sharing host state should drive dashboard overall state");

  lan::runtime::tests::RunDesktopRuntimeSnapshotTests();

  std::cout << "runtime controller tests passed\n";
  return 0;
}
