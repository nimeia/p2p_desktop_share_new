#include "core/runtime/shell_chrome_presenter.h"

#include "desktop_host/DesktopCommandIds.h"

namespace lan::runtime {

TrayShellCommandRoute ResolveTrayShellCommand(int id) {
  TrayShellCommandRoute route;
  route.handled = true;
  switch (id) {
    case ID_TRAY_OPEN_DASHBOARD:
      route.kind = TrayShellCommandKind::OpenDashboard;
      return route;
    case ID_TRAY_START_SHARING:
      route.kind = TrayShellCommandKind::StartSharing;
      return route;
    case ID_TRAY_STOP_SHARING:
      route.kind = TrayShellCommandKind::StopSharing;
      return route;
    case ID_TRAY_COPY_VIEWER_URL:
      route.kind = TrayShellCommandKind::CopyViewerUrl;
      return route;
    case ID_TRAY_SHOW_QR:
      route.kind = TrayShellCommandKind::ShowQr;
      return route;
    case ID_TRAY_OPEN_SHARE_WIZARD:
      route.kind = TrayShellCommandKind::OpenShareWizard;
      return route;
    case ID_TRAY_EXIT:
      route.kind = TrayShellCommandKind::ExitApp;
      return route;
    default:
      route.handled = false;
      route.kind = TrayShellCommandKind::None;
      return route;
  }
}

ShellChromeStatusViewModel BuildShellChromeStatusViewModel(const ShellChromeStateInput& input) {
  ShellChromeStatusViewModel viewModel;
  viewModel.statusText = L"Status: ";
  viewModel.statusText += input.serverRunning ? L"running" : L"stopped";

  viewModel.webStateText = L"Host Page: ";
  viewModel.webStateText += input.hostPageState.empty() ? L"unknown" : input.hostPageState;
  viewModel.webStateText += L" | WebView: ";
  viewModel.webStateText += input.webviewStatus.empty() ? L"unknown" : input.webviewStatus;
  viewModel.webStateText += L" | Hotspot: ";
  viewModel.webStateText += input.hotspotStatus.empty() ? L"unknown" : input.hotspotStatus;
  if (input.attentionNeeded) {
    viewModel.detailText = L"Attention needed";
  } else if (!input.serverRunning) {
    viewModel.detailText = L"The local sharing service is stopped.";
  } else if (input.viewerCount > 0) {
    viewModel.detailText = std::to_wstring(input.viewerCount) + L" viewer(s) connected.";
  } else {
    viewModel.detailText = L"Waiting for viewers.";
  }
  return viewModel;
}

TrayIconViewModel BuildTrayIconViewModel(const ShellChromeStateInput& input) {
  TrayIconViewModel viewModel;
  viewModel.tooltip = L"ViewMesh Host";
  if (input.serverRunning) {
    if (input.viewerCount > 0) {
      viewModel.tooltip += L" - Sharing (" + std::to_wstring(input.viewerCount) + L" viewer(s))";
      viewModel.statusBadge = std::to_wstring(input.viewerCount) + L" viewer(s)";
    } else if (input.attentionNeeded) {
      viewModel.tooltip += L" - Attention needed";
      viewModel.statusBadge = L"Needs attention";
    } else if (input.hostStateSharing) {
      viewModel.tooltip += L" - Sharing (waiting for viewers)";
      viewModel.statusBadge = L"Sharing";
    } else {
      viewModel.tooltip += L" - Ready to share";
      viewModel.statusBadge = L"Ready";
    }
  } else {
    viewModel.tooltip += L" - Ready to share";
    viewModel.statusBadge = L"Stopped";
  }

  if (input.trayBalloonPending) {
    viewModel.showBalloon = true;
    viewModel.balloonTitle = L"Running in tray";
    viewModel.balloonText = L"The window can stay hidden while the local sharing service remains available.";
  }
  return viewModel;
}

TrayMenuViewModel BuildTrayMenuViewModel(const ShellChromeStateInput& input) {
  TrayMenuViewModel viewModel;
  viewModel.showStartSharing = !input.serverRunning;
  viewModel.showStopSharing = input.serverRunning;
  viewModel.copyViewerUrlEnabled = input.viewerUrlAvailable;
  viewModel.showQrEnabled = input.shareActionsAvailable;
  viewModel.openShareWizardEnabled = input.shareActionsAvailable;
  return viewModel;
}

} // namespace lan::runtime
