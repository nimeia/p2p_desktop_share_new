#pragma once

#include "core/runtime/admin_view_model_assembler.h"
#include "core/runtime/diagnostics_view_model_assembler.h"
#include "core/runtime/host_action_coordinator.h"
#include "core/runtime/desktop_edit_session_presenter.h"

#include <array>
#include <cstddef>
#include <vector>

namespace lan::runtime {

enum class DesktopShellPage {
  None,
  Dashboard,
  Setup,
  Network,
  Sharing,
  Monitor,
  Diagnostics,
  Settings,
};

enum class DesktopShellCommandKind {
  None,
  NavigatePage,
  RetryShell,
  ShellOpenHost,
  RefreshFilteredLogs,
  EditSessionDraftChanged,
  ApplySessionTemplate,
  RefreshHostRuntime,
  EnsureHotspotDefaults,
  GenerateRoomToken,
  ExecuteHostAction,
  CopyHostUrl,
  CopyViewerUrl,
  SaveQrImage,
  DiagnosticsExportZip,
  CopyDiagnosticsPath,
  CopyDiagnosticsLogs,
  SaveDiagnosticsLogs,
  StartHotspot,
  StopHotspot,
  OpenWifiDirectPairing,
  OpenSystemHotspotSettings,
  OpenPairingHelp,
  SelectNetworkCandidate,
  DashboardSuggestionFix,
  DashboardSuggestionInfo,
  DashboardSuggestionSetup,
};

struct DesktopShellCommandRoute {
  bool handled = false;
  DesktopShellCommandKind kind = DesktopShellCommandKind::None;
  DesktopShellPage page = DesktopShellPage::None;
  HostActionKind hostAction = HostActionKind::StartServer;
  std::size_t index = 0;
};

struct NativeCommandButtonPolicyInput {
  bool serverRunning = false;
  bool hotspotRunning = false;
  bool hotspotSupported = false;
  bool shellStartEnabled = true;
};

struct NativeCommandButtonPolicy {
  bool startEnabled = true;
  bool stopEnabled = false;
  bool startHotspotEnabled = false;
  bool stopHotspotEnabled = false;
  bool shellStartEnabled = true;
};

struct DashboardButtonPolicy {
  bool primaryActionEnabled = true;
  std::array<bool, 4> suggestionFixEnabled{};
  std::array<bool, 4> suggestionInfoEnabled{};
  std::array<bool, 4> suggestionSetupEnabled{};
};

struct NetworkButtonPolicy {
  std::array<bool, 4> adapterSelectEnabled{};
};

DesktopShellCommandRoute ResolveDesktopShellCommand(int id);
NativeCommandButtonPolicy BuildNativeCommandButtonPolicy(const NativeCommandButtonPolicyInput& input);
DashboardButtonPolicy BuildDashboardButtonPolicy(const DashboardViewModel& viewModel, std::size_t slotCount = 4);
NetworkButtonPolicy BuildNetworkButtonPolicy(const std::array<bool, 4>& candidatePresent);

} // namespace lan::runtime
