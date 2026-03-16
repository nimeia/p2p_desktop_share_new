#pragma once

#include <string>
#include <vector>

namespace lan::runtime {

enum class NativeShellNotificationKind {
  HealthRecovered,
  HealthDegraded,
  ViewerCountChanged,
  ServerExitedUnexpectedly,
};

struct NativeShellRuntimeState {
  bool serverRunning = false;
  bool localHealthReady = false;
  bool attentionNeeded = false;
  bool stopRequested = false;
  std::size_t viewerCount = 0;
  std::wstring hostPageState;
  std::wstring detailText;
};

struct NativeShellAlertDebounceConfig {
  int healthStableSamples = 2;
  int viewerStableSamples = 2;
  int exitStableSamples = 2;
  int notificationCooldownTicks = 1;
};

struct NativeShellNotification {
  NativeShellNotificationKind kind = NativeShellNotificationKind::HealthRecovered;
  std::wstring title;
  std::wstring body;
};

struct NativeShellAlertMemory {
  bool initialized = false;

  bool stableHealthReady = false;
  bool stableServerRunning = false;
  std::size_t stableViewerCount = 0;

  bool pendingHealthReady = false;
  int pendingHealthSamples = 0;

  std::size_t pendingViewerCount = 0;
  int pendingViewerSamples = 0;

  bool pendingServerRunning = false;
  int pendingServerSamples = 0;

  int cooldownRemainingTicks = 0;
};

struct NativeShellAlertTickResult {
  NativeShellAlertMemory memory;
  std::vector<NativeShellNotification> notifications;
};

NativeShellAlertTickResult TickNativeShellAlerts(const NativeShellRuntimeState& runtime,
                                                 const NativeShellAlertMemory& memory,
                                                 const NativeShellAlertDebounceConfig& config = {});

} // namespace lan::runtime
