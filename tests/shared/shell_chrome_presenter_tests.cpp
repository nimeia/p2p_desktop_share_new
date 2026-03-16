#include "core/runtime/shell_chrome_presenter.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "shell chrome presenter test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto open = ResolveTrayShellCommand(1191);
  Expect(open.handled, "open dashboard tray command should be handled");
  Expect(open.kind == TrayShellCommandKind::OpenDashboard, "open dashboard should map correctly");

  const auto qr = ResolveTrayShellCommand(1195);
  Expect(qr.handled, "show qr tray command should be handled");
  Expect(qr.kind == TrayShellCommandKind::ShowQr, "show qr should map correctly");

  const auto unknown = ResolveTrayShellCommand(9999);
  Expect(!unknown.handled, "unknown tray command should not be handled");

  ShellChromeStateInput input;
  input.serverRunning = true;
  input.viewerCount = 2;
  input.trayBalloonPending = true;
  input.hostPageState = L"sharing";
  input.webviewStatus = L"ready";
  input.hotspotStatus = L"running";
  input.viewerUrlAvailable = true;
  input.shareActionsAvailable = true;

  const auto status = BuildShellChromeStatusViewModel(input);
  Expect(status.statusText == L"Status: running", "status text should reflect running state");
  Expect(status.webStateText.find(L"Host Page: sharing") != std::wstring::npos, "web state should include host page state");
  Expect(status.webStateText.find(L"Hotspot: running") != std::wstring::npos, "web state should include hotspot state");
  Expect(status.detailText == L"2 viewer(s) connected.", "detail text should reflect viewer count");

  const auto tray = BuildTrayIconViewModel(input);
  Expect(tray.tooltip.find(L"Sharing (2 viewer(s))") != std::wstring::npos, "tray tooltip should include viewer count");
  Expect(tray.showBalloon, "tray balloon should be shown when pending");
  Expect(tray.balloonTitle == L"Running in tray", "tray balloon title should match");
  Expect(tray.statusBadge == L"2 viewer(s)", "tray badge should include viewer count");

  const auto menu = BuildTrayMenuViewModel(input);
  Expect(menu.showStopSharing, "stop sharing should be visible while server is running");
  Expect(!menu.showStartSharing, "start sharing should be hidden while server is running");
  Expect(menu.copyViewerUrlEnabled, "copy viewer url should be enabled when available");
  Expect(menu.showQrEnabled, "show qr should be enabled when share actions are available");
  Expect(menu.openShareWizardEnabled, "share wizard should be enabled when share actions are available");

  input.serverRunning = false;
  input.viewerCount = 0;
  input.trayBalloonPending = false;
  input.viewerUrlAvailable = false;
  input.shareActionsAvailable = false;
  const auto stoppedTray = BuildTrayIconViewModel(input);
  Expect(stoppedTray.tooltip == L"LAN Screen Share Host - Ready to share", "stopped tooltip should show ready state");
  Expect(stoppedTray.statusBadge == L"Stopped", "stopped badge should match");
  Expect(!stoppedTray.showBalloon, "balloon should not show when not pending");
  const auto stoppedMenu = BuildTrayMenuViewModel(input);
  Expect(stoppedMenu.showStartSharing, "start sharing should be visible while stopped");
  Expect(!stoppedMenu.showStopSharing, "stop sharing should be hidden while stopped");
  Expect(!stoppedMenu.copyViewerUrlEnabled, "copy viewer url should be disabled when unavailable");

  std::cout << "shell chrome presenter tests passed\n";
  return 0;
}
