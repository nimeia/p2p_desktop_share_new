#include "core/runtime/host_observability_coordinator.h"

#include <iostream>

namespace {

bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << std::endl;
    return false;
  }
  return true;
}

} // namespace

int main() {
  using namespace lan::runtime;

  HostObservabilityState state;
  auto logResult = AppendHostObservabilityLog(state, L"10:00:00", L"Start failed: port blocked");
  if (!Expect(logResult.refreshDashboard, "native log should refresh dashboard")) return 1;
  if (!Expect(logResult.refreshDiagnostics, "native log should refresh diagnostics")) return 1;
  if (!Expect(logResult.state.lastErrorSummary == L"Start failed: port blocked", "error summary should follow latest blocking log")) return 1;
  if (!Expect(logResult.state.logEntries.size() == 1, "log entry should be recorded")) return 1;
  if (!Expect(logResult.state.logEntries.front().level == L"Error", "blocked failure should classify as error")) return 1;

  auto hostStatus = ParseShellBridgeInboundMessage(L"{\"kind\":\"status\",\"state\":\"ready\",\"viewers\":2}");
  auto statusResult = CoordinateHostStatusMessage(logResult.state, hostStatus, L"10:00:05");
  if (!Expect(statusResult.refreshShareInfo, "host status should request share refresh")) return 1;
  if (!Expect(statusResult.state.hostPageState == L"ready", "host state should update")) return 1;
  if (!Expect(statusResult.state.lastViewers == 2, "viewer count from host page should be captured")) return 1;
  if (!Expect(statusResult.state.timelineText.find(L"Host page loaded") != std::wstring::npos, "ready transition should append timeline event")) return 1;

  auto pollResult = CoordinateHostPollResult(statusResult.state, 200, 1, 0, L"10:00:10");
  if (!Expect(pollResult.refreshShareInfo, "poll result should refresh share info")) return 1;
  if (!Expect(pollResult.updateTrayIcon, "poll result should update tray icon")) return 1;
  if (!Expect(pollResult.state.lastRooms == 1 && pollResult.state.lastViewers == 0, "poll result should update room and viewer counts")) return 1;
  if (!Expect(pollResult.state.timelineText.find(L"Viewer disconnected") != std::wstring::npos, "viewer drop should append disconnect timeline")) return 1;
  if (!Expect(pollResult.statsText == L"Rooms: 1  Viewers: 0", "poll stats text should be normalized")) return 1;

  auto networkLog = AppendHostObservabilityLog(pollResult.state, L"10:00:11", L"Hotspot started: DemoNet");
  HostObservabilityFilter filter;
  filter.sourceFilter = L"network";
  const auto filtered = BuildHostObservabilityFilteredLogText(networkLog.state, filter);
  if (!Expect(filtered.find(L"Hotspot started: DemoNet") != std::wstring::npos, "network filter should retain network logs")) return 1;
  if (!Expect(filtered.find(L"Start failed") == std::wstring::npos, "network filter should remove non-network logs")) return 1;

  std::cout << "host observability coordinator tests passed" << std::endl;
  return 0;
}
