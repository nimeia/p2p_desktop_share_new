#pragma once

#include <string>

namespace lan::runtime {

enum class TrayShellCommandKind {
  None,
  OpenDashboard,
  StartSharing,
  StopSharing,
  CopyViewerUrl,
  ShowQr,
  OpenShareWizard,
  ExitApp,
};

struct TrayShellCommandRoute {
  bool handled = false;
  TrayShellCommandKind kind = TrayShellCommandKind::None;
};

struct ShellChromeStateInput {
  bool serverRunning = false;
  bool hotspotRunning = false;
  bool trayBalloonPending = false;
  bool hostStateSharing = false;
  bool attentionNeeded = false;
  bool viewerUrlAvailable = false;
  bool shareActionsAvailable = false;
  std::size_t viewerCount = 0;
  std::wstring hostPageState;
  std::wstring webviewStatus;
  std::wstring hotspotStatus;
};

struct ShellChromeStatusViewModel {
  std::wstring statusText;
  std::wstring webStateText;
  std::wstring detailText;
};

struct TrayIconViewModel {
  std::wstring tooltip;
  std::wstring statusBadge;
  bool showBalloon = false;
  std::wstring balloonTitle;
  std::wstring balloonText;
};

struct TrayMenuViewModel {
  bool showStartSharing = false;
  bool showStopSharing = false;
  bool copyViewerUrlEnabled = false;
  bool showQrEnabled = false;
  bool openShareWizardEnabled = false;
};

TrayShellCommandRoute ResolveTrayShellCommand(int id);
ShellChromeStatusViewModel BuildShellChromeStatusViewModel(const ShellChromeStateInput& input);
TrayIconViewModel BuildTrayIconViewModel(const ShellChromeStateInput& input);
TrayMenuViewModel BuildTrayMenuViewModel(const ShellChromeStateInput& input);

} // namespace lan::runtime
