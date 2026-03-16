#include "core/runtime/native_shell_alert_coordinator.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "native shell alert coordinator test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  NativeShellAlertMemory memory;
  NativeShellRuntimeState state;
  state.serverRunning = true;
  state.localHealthReady = true;
  state.viewerCount = 0;
  state.hostPageState = L"sharing";

  auto tick = TickNativeShellAlerts(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.empty(), "first sample should not notify");

  state.localHealthReady = false;
  state.detailText = L"Health probe failed.";
  tick = TickNativeShellAlerts(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.empty(), "first unhealthy sample should debounce");

  tick = TickNativeShellAlerts(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.size() == 1, "second unhealthy sample should notify");
  Expect(tick.notifications.front().kind == NativeShellNotificationKind::HealthDegraded,
         "health degraded kind should match");

  state.viewerCount = 2;
  tick = TickNativeShellAlerts(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.empty(), "viewer count should debounce");

  tick = TickNativeShellAlerts(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.size() == 1, "stable viewer count change should notify");
  Expect(tick.notifications.front().kind == NativeShellNotificationKind::ViewerCountChanged,
         "viewer count notification kind should match");

  state.serverRunning = false;
  state.stopRequested = false;
  state.detailText = L"Server exited unexpectedly.";
  tick = TickNativeShellAlerts(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.empty(), "server exit should debounce");

  tick = TickNativeShellAlerts(state, memory);
  Expect(tick.notifications.size() == 1, "unexpected exit should notify after debounce");
  Expect(tick.notifications.front().kind == NativeShellNotificationKind::ServerExitedUnexpectedly,
         "server exit kind should match");

  std::cout << "native shell alert coordinator tests passed\n";
  return 0;
}
