#include "core/runtime/host_shell_lifecycle_coordinator.h"

namespace lan::runtime {

HostShellLifecyclePlan CoordinateHostShellLifecycle(const HostShellLifecycleInput& input) {
  HostShellLifecyclePlan plan;

  switch (input.event) {
    case HostShellLifecycleEvent::StartupReady:
      plan.refreshHostRuntime = true;
      plan.createRuntimeTimer = true;
      plan.timerIntervalMs = input.timerIntervalMs > 0 ? input.timerIntervalMs : 1000;
      plan.createTrayIcon = true;
      plan.appendUiCreatedLog = true;
      plan.refreshShareInfo = true;
      plan.refreshDashboard = true;
      plan.setDashboardPage = true;
      plan.updateUiState = true;
      break;

    case HostShellLifecycleEvent::ShowRequested:
      plan.showWindow = true;
      plan.updateWindow = true;
      plan.ensureWebViewInitialized = true;
      plan.relayoutWindow = true;
      plan.refreshHtmlAdminPreview = true;
      plan.refreshShellFallback = true;
      plan.applyNativeCommandButtonPolicy = true;
      plan.updateTrayIcon = true;
      break;

    case HostShellLifecycleEvent::RestoreRequested:
      plan.setDashboardPage = true;
      plan.restoreWindow = true;
      plan.setForeground = true;
      plan.ensureWebViewInitialized = true;
      plan.relayoutWindow = true;
      plan.refreshHtmlAdminPreview = true;
      plan.refreshShellFallback = true;
      plan.applyNativeCommandButtonPolicy = true;
      plan.updateTrayIcon = true;
      break;

    case HostShellLifecycleEvent::MinimizeRequested:
      plan.hideWindow = true;
      plan.markTrayBalloonPending = input.showBalloon;
      plan.updateTrayIcon = input.showBalloon;
      break;

    case HostShellLifecycleEvent::CloseRequested:
      if (input.exitRequested) {
        plan.destroyWindow = true;
      } else {
        plan.hideWindow = true;
        plan.markTrayBalloonPending = true;
        plan.updateTrayIcon = true;
      }
      break;

    case HostShellLifecycleEvent::DestroyRequested:
      plan.killTimer = true;
      plan.removeTrayIcon = true;
      plan.stopServer = true;
      break;

    case HostShellLifecycleEvent::TrayExitRequested:
      plan.markExitRequested = true;
      plan.destroyWindow = true;
      break;
  }

  return plan;
}

} // namespace lan::runtime
