#include "core/runtime/host_observability_coordinator.h"

#include <algorithm>
#include <cwctype>
#include <sstream>

namespace lan::runtime {
namespace {

bool ContainsCaseInsensitive(std::wstring_view text, std::wstring_view needle) {
  std::wstring hay(text);
  std::wstring ndl(needle);
  std::transform(hay.begin(), hay.end(), hay.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(::towlower(ch));
  });
  std::transform(ndl.begin(), ndl.end(), ndl.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(::towlower(ch));
  });
  return hay.find(ndl) != std::wstring::npos;
}

std::wstring DetectLogLevel(std::wstring_view line) {
  if (ContainsCaseInsensitive(line, L"failed") || ContainsCaseInsensitive(line, L"error")) return L"Error";
  if (ContainsCaseInsensitive(line, L"warn") || ContainsCaseInsensitive(line, L"attention")) return L"Warning";
  return L"Info";
}

std::wstring DetectLogSource(std::wstring_view line) {
  if (ContainsCaseInsensitive(line, L"[host-page]") || ContainsCaseInsensitive(line, L"webview")) return L"webview";
  if (ContainsCaseInsensitive(line, L"network") || ContainsCaseInsensitive(line, L"hotspot") || ContainsCaseInsensitive(line, L"wifi")) return L"network";
  if (ContainsCaseInsensitive(line, L"[spawn]") || ContainsCaseInsensitive(line, L"/health") || ContainsCaseInsensitive(line, L"server")) return L"server";
  return L"app";
}

void TrimLogStorage(HostObservabilityState& state) {
  if (state.logText.size() > 32000) {
    state.logText.erase(0, state.logText.size() - 28000);
  }
  if (state.logEntries.size() > 400) {
    state.logEntries.erase(state.logEntries.begin(), state.logEntries.begin() + 120);
  }
}

void TrimTimelineStorage(HostObservabilityState& state) {
  if (state.timelineText.size() > 24000) {
    state.timelineText.erase(0, state.timelineText.size() - 20000);
  }
}

void AppendTimeline(HostObservabilityState& state, std::wstring_view timestamp, std::wstring_view eventText) {
  std::wstring line = L"[";
  line += timestamp;
  line += L"] ";
  line += eventText;
  line += L"\r\n";
  state.timelineText += line;
  TrimTimelineStorage(state);
}

} // namespace

HostObservabilityMutationResult AppendHostObservabilityLog(const HostObservabilityState& state,
                                                           std::wstring_view timestamp,
                                                           std::wstring_view line) {
  HostObservabilityMutationResult result;
  result.state = state;
  std::wstring entry = L"[";
  entry += timestamp;
  entry += L"] ";
  entry += line;
  entry += L"\r\n";
  result.state.logText += entry;
  result.state.logEntries.push_back(HostObservabilityLogEntry{std::wstring(timestamp), DetectLogLevel(line), DetectLogSource(line), std::wstring(line)});
  TrimLogStorage(result.state);
  if (ContainsCaseInsensitive(line, L"failed") || ContainsCaseInsensitive(line, L"error") ||
      ContainsCaseInsensitive(line, L"blocked") || ContainsCaseInsensitive(line, L"missing")) {
    result.state.lastErrorSummary = std::wstring(line);
  }
  result.refreshDashboard = true;
  result.refreshDiagnostics = true;
  return result;
}

HostObservabilityMutationResult AppendHostObservabilityTimelineEvent(const HostObservabilityState& state,
                                                                     std::wstring_view timestamp,
                                                                     std::wstring_view eventText) {
  HostObservabilityMutationResult result;
  result.state = state;
  AppendTimeline(result.state, timestamp, eventText);
  return result;
}

HostObservabilityMutationResult CoordinateHostStatusMessage(const HostObservabilityState& state,
                                                            const ShellBridgeInboundMessage& message,
                                                            std::wstring_view timestamp) {
  HostObservabilityMutationResult result;
  result.state = state;

  if (!message.hostState.empty()) {
    if (result.state.hostPageState != message.hostState) {
      if (message.hostState == L"loading") AppendTimeline(result.state, timestamp, L"Host page loading");
      else if (message.hostState == L"ready") AppendTimeline(result.state, timestamp, L"Host page loaded");
      else if (message.hostState == L"sharing") AppendTimeline(result.state, timestamp, L"Host page sharing started");
    }
    result.state.hostPageState = message.hostState;
  }

  if (message.hasCaptureState) {
    if (result.state.captureState != message.captureState) {
      if (message.captureState == L"selecting") {
        AppendTimeline(result.state, timestamp, L"Capture picker opened");
      } else if (message.captureState == L"active") {
        AppendTimeline(result.state, timestamp, L"Capture source selected");
      } else if (message.captureState == L"idle") {
        AppendTimeline(result.state, timestamp, L"Capture stopped");
      } else if (!message.captureState.empty()) {
        AppendTimeline(result.state, timestamp, std::wstring(L"Capture state: ") + message.captureState);
      }
    }
    result.state.captureState = message.captureState;
  }

  if (message.hasCaptureLabel) {
    if (result.state.captureLabel != message.captureLabel) {
      if (!message.captureLabel.empty()) {
        AppendTimeline(result.state, timestamp, std::wstring(L"Capture target: ") + message.captureLabel);
      } else if (!result.state.captureLabel.empty()) {
        AppendTimeline(result.state, timestamp, L"Capture target cleared");
      }
    }
    result.state.captureLabel = message.captureLabel;
  }

  if (message.hasViewers) {
    result.state.lastViewers = message.viewers;
  }

  result.refreshShareInfoLightweight = true;
  return result;
}

HostObservabilityMutationResult CoordinateHostPollResult(const HostObservabilityState& state,
                                                         long status,
                                                         std::size_t rooms,
                                                         std::size_t viewers,
                                                         std::wstring_view timestamp) {
  HostObservabilityMutationResult result;
  result.state = state;

  std::wstringstream ss;
  if (status == 200) {
    if (result.state.lastViewers == 0 && viewers > 0) {
      AppendTimeline(result.state, timestamp, L"First viewer connected");
    } else if (result.state.lastViewers > 0 && viewers == 0) {
      AppendTimeline(result.state, timestamp, L"Viewer disconnected");
    }
    result.state.lastRooms = rooms;
    result.state.lastViewers = viewers;
    ss << L"Rooms: " << rooms << L"  Viewers: " << viewers;
  } else {
    ss << L"Rooms: -  Viewers: - (status=" << status << L")";
  }

  result.statsText = ss.str();
  if (result.state.lastViewers > 0) {
    result.state.handoffDelivered = true;
  }
  result.refreshShareInfo = true;
  result.updateTrayIcon = true;
  return result;
}

std::wstring BuildHostObservabilityFilteredLogText(const HostObservabilityState& state,
                                                   const HostObservabilityFilter& filter) {
  std::wstringstream ss;
  for (const auto& entry : state.logEntries) {
    if (!filter.levelFilter.empty() && entry.level != filter.levelFilter) continue;
    if (!filter.sourceFilter.empty() && entry.source != filter.sourceFilter) continue;
    if (!filter.searchText.empty() && !ContainsCaseInsensitive(entry.message, filter.searchText)) continue;
    ss << L"[" << entry.timestamp << L"][" << entry.level << L"][" << entry.source << L"] " << entry.message << L"\r\n";
  }
  return ss.str();
}

} // namespace lan::runtime
