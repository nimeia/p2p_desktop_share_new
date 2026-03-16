#include "core/runtime/host_shell_lifecycle_coordinator.h"

#include <iostream>

using lan::runtime::CoordinateHostShellLifecycle;
using lan::runtime::HostShellLifecycleEvent;
using lan::runtime::HostShellLifecycleInput;

namespace {

bool Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << std::endl;
    return false;
  }
  return true;
}

bool TestStartupPlan() {
  const auto plan = CoordinateHostShellLifecycle({
    .event = HostShellLifecycleEvent::StartupReady,
    .timerIntervalMs = 1500,
  });
  return Expect(plan.refreshHostRuntime, "startup should refresh host runtime") &&
         Expect(plan.createRuntimeTimer, "startup should create timer") &&
         Expect(plan.timerIntervalMs == 1500, "startup should preserve timer interval") &&
         Expect(plan.createTrayIcon, "startup should create tray icon") &&
         Expect(plan.appendUiCreatedLog, "startup should append creation log") &&
         Expect(plan.setDashboardPage, "startup should default to dashboard") &&
         Expect(plan.updateUiState, "startup should refresh ui");
}

bool TestShowPlan() {
  const auto plan = CoordinateHostShellLifecycle({.event = HostShellLifecycleEvent::ShowRequested});
  return Expect(plan.showWindow, "show should show window") &&
         Expect(plan.updateWindow, "show should update window") &&
         Expect(plan.ensureWebViewInitialized, "show should ensure webview") &&
         Expect(plan.relayoutWindow, "show should relayout") &&
         Expect(plan.refreshHtmlAdminPreview, "show should refresh html admin") &&
         Expect(plan.applyNativeCommandButtonPolicy, "show should refresh button policy") &&
         Expect(plan.updateTrayIcon, "show should refresh tray icon");
}

bool TestRestorePlan() {
  const auto plan = CoordinateHostShellLifecycle({.event = HostShellLifecycleEvent::RestoreRequested});
  return Expect(plan.setDashboardPage, "restore should return to dashboard") &&
         Expect(plan.restoreWindow, "restore should restore window") &&
         Expect(plan.setForeground, "restore should foreground window") &&
         Expect(plan.ensureWebViewInitialized, "restore should ensure webview") &&
         Expect(plan.relayoutWindow, "restore should relayout") &&
         Expect(plan.refreshShellFallback, "restore should refresh shell fallback");
}

bool TestMinimizePlan() {
  const auto plan = CoordinateHostShellLifecycle({
    .event = HostShellLifecycleEvent::MinimizeRequested,
    .showBalloon = true,
  });
  return Expect(plan.hideWindow, "minimize should hide window") &&
         Expect(plan.markTrayBalloonPending, "minimize with balloon should mark balloon pending") &&
         Expect(plan.updateTrayIcon, "minimize with balloon should update tray icon");
}

bool TestClosePlanWithoutExit() {
  const auto plan = CoordinateHostShellLifecycle({
    .event = HostShellLifecycleEvent::CloseRequested,
    .exitRequested = false,
  });
  return Expect(plan.hideWindow, "close without exit should hide window") &&
         Expect(plan.markTrayBalloonPending, "close without exit should show tray balloon") &&
         Expect(!plan.destroyWindow, "close without exit should not destroy window");
}

bool TestClosePlanWithExit() {
  const auto plan = CoordinateHostShellLifecycle({
    .event = HostShellLifecycleEvent::CloseRequested,
    .exitRequested = true,
  });
  return Expect(plan.destroyWindow, "close with exit should destroy window") &&
         Expect(!plan.hideWindow, "close with exit should not hide window first");
}

bool TestDestroyPlan() {
  const auto plan = CoordinateHostShellLifecycle({.event = HostShellLifecycleEvent::DestroyRequested});
  return Expect(plan.killTimer, "destroy should kill timer") &&
         Expect(plan.removeTrayIcon, "destroy should remove tray icon") &&
         Expect(plan.stopServer, "destroy should stop server");
}

bool TestTrayExitPlan() {
  const auto plan = CoordinateHostShellLifecycle({.event = HostShellLifecycleEvent::TrayExitRequested});
  return Expect(plan.markExitRequested, "tray exit should mark exit requested") &&
         Expect(plan.destroyWindow, "tray exit should destroy window");
}

} // namespace

int main() {
  if (!TestStartupPlan()) return 1;
  if (!TestShowPlan()) return 1;
  if (!TestRestorePlan()) return 1;
  if (!TestMinimizePlan()) return 1;
  if (!TestClosePlanWithoutExit()) return 1;
  if (!TestClosePlanWithExit()) return 1;
  if (!TestDestroyPlan()) return 1;
  if (!TestTrayExitPlan()) return 1;
  std::cout << "host shell lifecycle coordinator tests passed" << std::endl;
  return 0;
}
