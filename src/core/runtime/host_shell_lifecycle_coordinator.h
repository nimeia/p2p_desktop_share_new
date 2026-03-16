#pragma once

namespace lan::runtime {

enum class HostShellLifecycleEvent {
  StartupReady,
  ShowRequested,
  RestoreRequested,
  MinimizeRequested,
  CloseRequested,
  DestroyRequested,
  TrayExitRequested,
};

struct HostShellLifecycleInput {
  HostShellLifecycleEvent event = HostShellLifecycleEvent::StartupReady;
  bool exitRequested = false;
  bool showBalloon = false;
  int timerIntervalMs = 1000;
};

struct HostShellLifecyclePlan {
  bool refreshHostRuntime = false;
  bool createRuntimeTimer = false;
  int timerIntervalMs = 0;
  bool createTrayIcon = false;
  bool appendUiCreatedLog = false;
  bool refreshShareInfo = false;
  bool refreshDashboard = false;
  bool setDashboardPage = false;
  bool updateUiState = false;

  bool showWindow = false;
  bool restoreWindow = false;
  bool updateWindow = false;
  bool setForeground = false;
  bool ensureWebViewInitialized = false;
  bool relayoutWindow = false;
  bool refreshHtmlAdminPreview = false;
  bool refreshShellFallback = false;
  bool applyNativeCommandButtonPolicy = false;
  bool updateTrayIcon = false;

  bool hideWindow = false;
  bool markTrayBalloonPending = false;

  bool killTimer = false;
  bool removeTrayIcon = false;
  bool stopServer = false;

  bool markExitRequested = false;
  bool destroyWindow = false;
};

HostShellLifecyclePlan CoordinateHostShellLifecycle(const HostShellLifecycleInput& input);

} // namespace lan::runtime
