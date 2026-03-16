#include "core/runtime/native_shell_status_tracker.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "native shell status tracker test failed: " << message << "\n";
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
  state.viewerCount = 1;
  state.hostPageState = L"sharing";
  state.detailText = L"Healthy";

  auto tick = TickNativeShellStatusTracker(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.empty(), "first tick should not notify");
  Expect(tick.statusViewModel.statusText == L"Status: running", "running status text should match");
  Expect(tick.trayIconViewModel.statusBadge == L"1 viewer(s)", "badge should reflect viewer count");
  Expect(tick.trayMenuViewModel.showStopSharing, "stop sharing should be visible while stable running");

  state.localHealthReady = false;
  state.attentionNeeded = true;
  state.detailText = L"Probe failing";
  tick = TickNativeShellStatusTracker(state, memory);
  memory = tick.memory;
  Expect(tick.notifications.empty(), "degraded state should debounce");
  tick = TickNativeShellStatusTracker(state, memory);
  Expect(tick.notifications.size() == 1, "degraded state should eventually notify");
  Expect(tick.trayIconViewModel.statusBadge == L"1 viewer(s)" || tick.trayIconViewModel.statusBadge == L"Needs attention",
         "badge should remain meaningful after degradation");

  std::cout << "native shell status tracker tests passed\n";
  return 0;
}
