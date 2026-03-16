#include "core/runtime/desktop_shell_presenter.h"

#include "desktop_host/DesktopCommandIds.h"

#include <algorithm>

namespace lan::runtime {

DesktopShellCommandRoute ResolveDesktopShellCommand(int id) {
  DesktopShellCommandRoute route;
  route.handled = true;

  switch (id) {
    case ID_BTN_NAV_DASHBOARD:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Dashboard;
      return route;
    case ID_BTN_NAV_SETUP:
    case ID_BTN_DASH_CONTINUE:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Setup;
      return route;
    case ID_BTN_NAV_NETWORK:
    case ID_BTN_MANUAL_SELECT_IP:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Network;
      return route;
    case ID_BTN_NAV_SHARING:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Sharing;
      return route;
    case ID_BTN_NAV_MONITOR:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Monitor;
      return route;
    case ID_BTN_NAV_DIAGNOSTICS:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Diagnostics;
      return route;
    case ID_BTN_NAV_SETTINGS:
      route.kind = DesktopShellCommandKind::NavigatePage;
      route.page = DesktopShellPage::Settings;
      return route;
    case ID_BTN_SHELL_RETRY:
      route.kind = DesktopShellCommandKind::RetryShell;
      return route;
    case ID_BTN_SHELL_OPEN_HOST:
      route.kind = DesktopShellCommandKind::ShellOpenHost;
      return route;
    case ID_EDIT_DIAG_LOG_SEARCH:
    case ID_COMBO_DIAG_LEVEL:
    case ID_COMBO_DIAG_SOURCE:
      route.kind = DesktopShellCommandKind::RefreshFilteredLogs;
      return route;
    case ID_EDIT_SESSION_ROOM:
    case ID_EDIT_SESSION_TOKEN:
    case ID_EDIT_SESSION_BIND:
    case ID_EDIT_SESSION_PORT:
      route.kind = DesktopShellCommandKind::EditSessionDraftChanged;
      return route;
    case ID_COMBO_SESSION_TEMPLATE:
      route.kind = DesktopShellCommandKind::ApplySessionTemplate;
      return route;
    case ID_BTN_REFRESH_IP:
    case ID_BTN_REFRESH_NETWORK:
      route.kind = DesktopShellCommandKind::RefreshHostRuntime;
      return route;
    case ID_BTN_AUTO_HOTSPOT:
      route.kind = DesktopShellCommandKind::EnsureHotspotDefaults;
      return route;
    case ID_BTN_GENERATE:
      route.kind = DesktopShellCommandKind::GenerateRoomToken;
      return route;
    case ID_BTN_DASH_START:
    case ID_BTN_START:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::StartServer;
      return route;
    case ID_BTN_STOP:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::StopServer;
      return route;
    case ID_BTN_RESTART:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::RestartServer;
      return route;
    case ID_BTN_SERVICE_ONLY:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::StartServiceOnly;
      return route;
    case ID_BTN_START_AND_OPEN_HOST:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::StartAndOpenHost;
      return route;
    case ID_BTN_DASH_WIZARD:
    case ID_BTN_SHOW_WIZARD:
    case ID_BTN_OPEN_SHARE_WIZARD_2:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::ShowShareWizard;
      return route;
    case ID_BTN_COPY_HOST_URL:
      route.kind = DesktopShellCommandKind::CopyHostUrl;
      return route;
    case ID_BTN_OPEN_HOST:
    case ID_BTN_OPEN_HOST_BROWSER:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::OpenHostPage;
      return route;
    case ID_BTN_OPEN_VIEWER:
    case ID_BTN_OPEN_VIEWER_BROWSER:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::OpenViewerPage;
      return route;
    case ID_BTN_COPY_VIEWER:
    case ID_BTN_COPY_VIEWER_URL_2:
      route.kind = DesktopShellCommandKind::CopyViewerUrl;
      return route;
    case ID_BTN_SHOW_QR:
    case ID_BTN_FULLSCREEN_QR:
    case ID_BTN_OPEN_SHARE_CARD_2:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::ShowQr;
      return route;
    case ID_BTN_EXPORT_BUNDLE:
    case ID_BTN_EXPORT_OFFLINE_ZIP:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::ExportShareBundle;
      return route;
    case ID_BTN_OPEN_BUNDLE_FOLDER_2:
    case ID_BTN_DIAG_OPEN_OUTPUT:
    case ID_BTN_OPEN_FOLDER:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::OpenOutputFolder;
      return route;
    case ID_BTN_SAVE_QR_IMAGE:
      route.kind = DesktopShellCommandKind::SaveQrImage;
      return route;
    case ID_BTN_DIAG_OPEN_REPORT:
    case ID_BTN_OPEN_DIAGNOSTICS:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::OpenDiagnosticsReport;
      return route;
    case ID_BTN_DIAG_EXPORT_ZIP:
      route.kind = DesktopShellCommandKind::DiagnosticsExportZip;
      return route;
    case ID_BTN_DIAG_COPY_PATH:
      route.kind = DesktopShellCommandKind::CopyDiagnosticsPath;
      return route;
    case ID_BTN_DIAG_REFRESH_BUNDLE:
    case ID_BTN_REFRESH_CHECKS:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::RefreshDiagnosticsBundle;
      return route;
    case ID_BTN_DIAG_COPY_LOGS:
      route.kind = DesktopShellCommandKind::CopyDiagnosticsLogs;
      return route;
    case ID_BTN_DIAG_SAVE_LOGS:
      route.kind = DesktopShellCommandKind::SaveDiagnosticsLogs;
      return route;
    case ID_BTN_RUN_SELF_CHECK:
      route.kind = DesktopShellCommandKind::ExecuteHostAction;
      route.hostAction = HostActionKind::RunDesktopSelfCheck;
      return route;
    case ID_BTN_START_HOTSPOT:
      route.kind = DesktopShellCommandKind::StartHotspot;
      return route;
    case ID_BTN_STOP_HOTSPOT:
      route.kind = DesktopShellCommandKind::StopHotspot;
      return route;
    case ID_BTN_WIFI_DIRECT_PAIR:
    case ID_BTN_OPEN_CONNECTED_DEVICES:
      route.kind = DesktopShellCommandKind::OpenWifiDirectPairing;
      return route;
    case ID_BTN_OPEN_HOTSPOT_SETTINGS:
      route.kind = DesktopShellCommandKind::OpenSystemHotspotSettings;
      return route;
    case ID_BTN_OPEN_PAIRING_HELP:
      route.kind = DesktopShellCommandKind::OpenPairingHelp;
      return route;
    default:
      break;
  }

  if (id >= ID_BTN_SELECT_ADAPTER_1 && id <= ID_BTN_SELECT_ADAPTER_4) {
    route.kind = DesktopShellCommandKind::SelectNetworkCandidate;
    route.index = static_cast<std::size_t>(id - ID_BTN_SELECT_ADAPTER_1);
    return route;
  }
  if (id >= ID_BTN_DASH_SUGGESTION_FIX_1 && id <= ID_BTN_DASH_SUGGESTION_FIX_4) {
    route.kind = DesktopShellCommandKind::DashboardSuggestionFix;
    route.index = static_cast<std::size_t>((id - ID_BTN_DASH_SUGGESTION_FIX_1) / 10);
    return route;
  }
  if (id >= ID_BTN_DASH_SUGGESTION_INFO_1 && id <= ID_BTN_DASH_SUGGESTION_INFO_4) {
    route.kind = DesktopShellCommandKind::DashboardSuggestionInfo;
    route.index = static_cast<std::size_t>((id - ID_BTN_DASH_SUGGESTION_INFO_1) / 10);
    return route;
  }
  if (id >= ID_BTN_DASH_SUGGESTION_SETUP_1 && id <= ID_BTN_DASH_SUGGESTION_SETUP_4) {
    route.kind = DesktopShellCommandKind::DashboardSuggestionSetup;
    route.index = static_cast<std::size_t>((id - ID_BTN_DASH_SUGGESTION_SETUP_1) / 10);
    return route;
  }

  route.handled = false;
  route.kind = DesktopShellCommandKind::None;
  route.page = DesktopShellPage::None;
  return route;
}

NativeCommandButtonPolicy BuildNativeCommandButtonPolicy(const NativeCommandButtonPolicyInput& input) {
  NativeCommandButtonPolicy policy;
  policy.startEnabled = !input.serverRunning;
  policy.stopEnabled = input.serverRunning;
  policy.startHotspotEnabled = !input.hotspotRunning && input.hotspotSupported;
  policy.stopHotspotEnabled = input.hotspotRunning;
  policy.shellStartEnabled = input.shellStartEnabled;
  return policy;
}

DashboardButtonPolicy BuildDashboardButtonPolicy(const DashboardViewModel& viewModel, std::size_t slotCount) {
  DashboardButtonPolicy policy;
  policy.primaryActionEnabled = viewModel.primaryActionEnabled;

  const std::size_t limit = std::min<std::size_t>(policy.suggestionFixEnabled.size(), slotCount);
  std::size_t slot = 0;
  for (const auto& suggestion : viewModel.suggestions) {
    if (slot >= limit) break;
    policy.suggestionFixEnabled[slot] = suggestion.fixEnabled;
    policy.suggestionInfoEnabled[slot] = suggestion.infoEnabled;
    policy.suggestionSetupEnabled[slot] = suggestion.setupEnabled;
    ++slot;
  }
  while (slot < limit) {
    policy.suggestionFixEnabled[slot] = false;
    policy.suggestionInfoEnabled[slot] = true;
    policy.suggestionSetupEnabled[slot] = true;
    ++slot;
  }
  return policy;
}

NetworkButtonPolicy BuildNetworkButtonPolicy(const std::array<bool, 4>& candidatePresent) {
  NetworkButtonPolicy policy;
  policy.adapterSelectEnabled = candidatePresent;
  return policy;
}

} // namespace lan::runtime
