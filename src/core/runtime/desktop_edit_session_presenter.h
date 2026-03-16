#pragma once

#include "core/runtime/host_session_coordinator.h"

#include <string>

namespace lan::runtime {

enum class DesktopSessionTemplateKind {
  QuickShare = 0,
  FixedRoom = 1,
  DemoMode = 2,
};

struct DesktopEditSessionDraft {
  std::wstring bindAddress;
  std::wstring portText;
  std::wstring room;
  std::wstring token;
  int templateIndex = 0;
};

struct DesktopEditSessionInput {
  HostSessionState appliedState;
  DesktopEditSessionDraft draft;
  bool serverRunning = false;
  std::wstring hostIp;
  std::wstring outputDir;
  std::wstring shareBundleDir;
};

struct DesktopEditSessionButtonPolicy {
  bool generateEnabled = true;
  bool restartEnabled = true;
  bool serviceOnlyEnabled = true;
  bool startAndOpenHostEnabled = true;
};

struct DesktopEditSessionViewModel {
  HostSessionState draftState;
  bool dirty = false;
  bool pendingApply = false;
  bool portValid = true;
  std::wstring statusText;
  std::wstring sessionSummaryLabel;
  std::wstring sessionSummaryBody;
  std::wstring startButtonLabel;
  std::wstring restartButtonLabel;
  std::wstring serviceOnlyButtonLabel;
  std::wstring startAndOpenHostButtonLabel;
  DesktopEditSessionButtonPolicy buttonPolicy;
};

DesktopEditSessionDraft BuildDesktopEditSessionDraft(const HostSessionState& state,
                                                     int templateIndex = static_cast<int>(DesktopSessionTemplateKind::QuickShare));
HostSessionMutationResult ApplyDesktopEditSessionDraft(const HostSessionState& appliedState,
                                                       const DesktopEditSessionDraft& draft,
                                                       bool resetDeliveryState = false,
                                                       bool* portValid = nullptr);
DesktopEditSessionDraft ApplyDesktopSessionTemplate(const HostSessionState& baseState,
                                                    int templateIndex,
                                                    int preserveTemplateIndex = -1);
DesktopEditSessionViewModel BuildDesktopEditSessionViewModel(const DesktopEditSessionInput& input);

} // namespace lan::runtime
