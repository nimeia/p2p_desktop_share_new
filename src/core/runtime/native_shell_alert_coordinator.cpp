#include "core/runtime/native_shell_alert_coordinator.h"

#include <algorithm>
#include <utility>

namespace lan::runtime {
namespace {

std::wstring BuildViewerBody(std::size_t viewerCount) {
  if (viewerCount == 0) {
    return L"No viewers are connected right now.";
  }
  if (viewerCount == 1) {
    return L"1 viewer is connected.";
  }
  return std::to_wstring(viewerCount) + L" viewers are connected.";
}

void AddNotification(std::vector<NativeShellNotification>& notifications,
                     NativeShellAlertMemory& memory,
                     const NativeShellAlertDebounceConfig& config,
                     NativeShellNotificationKind kind,
                     std::wstring title,
                     std::wstring body) {
  if (memory.cooldownRemainingTicks > 0) {
    return;
  }
  notifications.push_back({kind, std::move(title), std::move(body)});
  memory.cooldownRemainingTicks = std::max(0, config.notificationCooldownTicks);
}

} // namespace

NativeShellAlertTickResult TickNativeShellAlerts(const NativeShellRuntimeState& runtime,
                                                 const NativeShellAlertMemory& inputMemory,
                                                 const NativeShellAlertDebounceConfig& config) {
  NativeShellAlertTickResult result;
  result.memory = inputMemory;

  if (result.memory.cooldownRemainingTicks > 0) {
    --result.memory.cooldownRemainingTicks;
  }

  if (!result.memory.initialized) {
    result.memory.initialized = true;
    result.memory.stableHealthReady = runtime.localHealthReady;
    result.memory.stableServerRunning = runtime.serverRunning;
    result.memory.stableViewerCount = runtime.viewerCount;
    result.memory.pendingHealthReady = runtime.localHealthReady;
    result.memory.pendingViewerCount = runtime.viewerCount;
    result.memory.pendingServerRunning = runtime.serverRunning;
    result.memory.pendingHealthSamples = 0;
    result.memory.pendingViewerSamples = 0;
    result.memory.pendingServerSamples = 0;
    return result;
  }

  if (runtime.localHealthReady == result.memory.stableHealthReady) {
    result.memory.pendingHealthSamples = 0;
  } else if (runtime.localHealthReady == result.memory.pendingHealthReady) {
    ++result.memory.pendingHealthSamples;
  } else {
    result.memory.pendingHealthReady = runtime.localHealthReady;
    result.memory.pendingHealthSamples = 1;
  }

  if (result.memory.pendingHealthSamples >= std::max(1, config.healthStableSamples)) {
    const bool previous = result.memory.stableHealthReady;
    result.memory.stableHealthReady = result.memory.pendingHealthReady;
    result.memory.pendingHealthSamples = 0;
    if (result.memory.stableHealthReady != previous) {
      if (result.memory.stableHealthReady) {
        AddNotification(result.notifications,
                        result.memory,
                        config,
                        NativeShellNotificationKind::HealthRecovered,
                        L"Sharing service recovered",
                        L"The local sharing service is healthy again.");
      } else {
        AddNotification(result.notifications,
                        result.memory,
                        config,
                        NativeShellNotificationKind::HealthDegraded,
                        L"Sharing service needs attention",
                        runtime.detailText.empty() ? L"The local sharing service health probe is failing." : runtime.detailText);
      }
    }
  }

  if (runtime.viewerCount == result.memory.stableViewerCount) {
    result.memory.pendingViewerSamples = 0;
  } else if (runtime.viewerCount == result.memory.pendingViewerCount) {
    ++result.memory.pendingViewerSamples;
  } else {
    result.memory.pendingViewerCount = runtime.viewerCount;
    result.memory.pendingViewerSamples = 1;
  }

  if (result.memory.pendingViewerSamples >= std::max(1, config.viewerStableSamples)) {
    const std::size_t previous = result.memory.stableViewerCount;
    result.memory.stableViewerCount = result.memory.pendingViewerCount;
    result.memory.pendingViewerSamples = 0;
    if (result.memory.stableViewerCount != previous) {
      AddNotification(result.notifications,
                      result.memory,
                      config,
                      NativeShellNotificationKind::ViewerCountChanged,
                      L"Viewer count changed",
                      BuildViewerBody(result.memory.stableViewerCount));
    }
  }

  if (runtime.serverRunning == result.memory.stableServerRunning) {
    result.memory.pendingServerSamples = 0;
  } else if (runtime.serverRunning == result.memory.pendingServerRunning) {
    ++result.memory.pendingServerSamples;
  } else {
    result.memory.pendingServerRunning = runtime.serverRunning;
    result.memory.pendingServerSamples = 1;
  }

  if (result.memory.pendingServerSamples >= std::max(1, config.exitStableSamples)) {
    const bool previous = result.memory.stableServerRunning;
    result.memory.stableServerRunning = result.memory.pendingServerRunning;
    result.memory.pendingServerSamples = 0;
    if (previous && !result.memory.stableServerRunning && !runtime.stopRequested) {
      AddNotification(result.notifications,
                      result.memory,
                      config,
                      NativeShellNotificationKind::ServerExitedUnexpectedly,
                      L"Sharing service stopped",
                      runtime.detailText.empty() ? L"The local sharing service exited unexpectedly." : runtime.detailText);
    }
  }

  return result;
}

} // namespace lan::runtime
