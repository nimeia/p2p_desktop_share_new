#include "core/runtime/native_shell_status_tracker.h"

namespace lan::runtime {

NativeShellStatusTrackerResult TickNativeShellStatusTracker(const NativeShellRuntimeState& runtime,
                                                            const NativeShellAlertMemory& memory,
                                                            const NativeShellAlertDebounceConfig& config) {
  NativeShellStatusTrackerResult result;
  const auto alertTick = TickNativeShellAlerts(runtime, memory, config);
  result.memory = alertTick.memory;
  result.notifications = alertTick.notifications;

  result.chromeInput.serverRunning = result.memory.stableServerRunning;
  result.chromeInput.hostStateSharing = result.memory.stableServerRunning;
  result.chromeInput.attentionNeeded = runtime.attentionNeeded || !result.memory.stableHealthReady;
  result.chromeInput.viewerUrlAvailable = result.memory.stableServerRunning;
  result.chromeInput.shareActionsAvailable = result.memory.stableServerRunning;
  result.chromeInput.viewerCount = result.memory.stableViewerCount;
  result.chromeInput.hostPageState = runtime.hostPageState.empty()
                                      ? (result.memory.stableServerRunning ? L"sharing" : L"stopped")
                                      : runtime.hostPageState;
  result.chromeInput.webviewStatus = result.memory.stableHealthReady ? L"healthy" : L"degraded";
  result.chromeInput.hotspotStatus = runtime.detailText;

  result.statusViewModel = BuildShellChromeStatusViewModel(result.chromeInput);
  result.trayIconViewModel = BuildTrayIconViewModel(result.chromeInput);
  result.trayMenuViewModel = BuildTrayMenuViewModel(result.chromeInput);
  return result;
}

} // namespace lan::runtime
