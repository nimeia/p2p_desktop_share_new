#pragma once

#include "core/runtime/native_shell_alert_coordinator.h"
#include "core/runtime/shell_chrome_presenter.h"

namespace lan::runtime {

struct NativeShellStatusTrackerResult {
  NativeShellAlertMemory memory;
  ShellChromeStateInput chromeInput;
  ShellChromeStatusViewModel statusViewModel;
  TrayIconViewModel trayIconViewModel;
  TrayMenuViewModel trayMenuViewModel;
  std::vector<NativeShellNotification> notifications;
};

NativeShellStatusTrackerResult TickNativeShellStatusTracker(const NativeShellRuntimeState& runtime,
                                                            const NativeShellAlertMemory& memory,
                                                            const NativeShellAlertDebounceConfig& config = {});

} // namespace lan::runtime
