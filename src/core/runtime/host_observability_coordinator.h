#pragma once

#include "core/runtime/shell_bridge_presenter.h"

#include <cstddef>
#include <string>
#include <vector>

namespace lan::runtime {

struct HostObservabilityLogEntry {
  std::wstring timestamp;
  std::wstring level;
  std::wstring source;
  std::wstring message;
};

struct HostObservabilityState {
  std::wstring logText;
  std::wstring timelineText;
  std::wstring lastErrorSummary;
  std::wstring hostPageState = L"idle";
  std::size_t lastRooms = 0;
  std::size_t lastViewers = 0;
  bool handoffDelivered = false;
  std::vector<HostObservabilityLogEntry> logEntries;
};

struct HostObservabilityMutationResult {
  HostObservabilityState state;
  bool refreshShareInfo = false;
  bool refreshDashboard = false;
  bool refreshDiagnostics = false;
  bool updateTrayIcon = false;
  std::wstring statsText;
};

struct HostObservabilityFilter {
  std::wstring searchText;
  std::wstring levelFilter;
  std::wstring sourceFilter;
};

HostObservabilityMutationResult AppendHostObservabilityLog(const HostObservabilityState& state,
                                                           std::wstring_view timestamp,
                                                           std::wstring_view line);
HostObservabilityMutationResult AppendHostObservabilityTimelineEvent(const HostObservabilityState& state,
                                                                     std::wstring_view timestamp,
                                                                     std::wstring_view eventText);
HostObservabilityMutationResult CoordinateHostStatusMessage(const HostObservabilityState& state,
                                                            const ShellBridgeInboundMessage& message,
                                                            std::wstring_view timestamp);
HostObservabilityMutationResult CoordinateHostPollResult(const HostObservabilityState& state,
                                                         long status,
                                                         std::size_t rooms,
                                                         std::size_t viewers,
                                                         std::wstring_view timestamp);
std::wstring BuildHostObservabilityFilteredLogText(const HostObservabilityState& state,
                                                   const HostObservabilityFilter& filter);

} // namespace lan::runtime
