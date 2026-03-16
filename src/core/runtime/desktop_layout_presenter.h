#pragma once

namespace lan::runtime {

enum class DesktopLayoutPage {
  Dashboard,
  Setup,
  Network,
  Sharing,
  Monitor,
  Diagnostics,
  Settings,
};

enum class DesktopLayoutSurfaceMode {
  Hidden,
  HostPreview,
  HtmlAdminPreview,
};

struct DesktopLayoutStateInput {
  DesktopLayoutPage currentPage = DesktopLayoutPage::Dashboard;
  bool preferHtmlAdminUi = false;
  bool serverRunning = false;
  bool webviewReady = false;
};

struct DesktopPageVisibility {
  bool showNativeNavigation = true;
  bool showDashboardPage = false;
  bool showSetupPage = false;
  bool showNetworkPage = false;
  bool showSharingPage = false;
  bool showMonitorPage = false;
  bool showDiagnosticsPage = false;
  bool showSettingsPage = false;
  bool showHostPreviewPlaceholder = false;
  bool showOpenHostButton = false;
};

struct DesktopLayoutRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

struct DesktopLayoutGeometry {
  DesktopLayoutRect webview;
  DesktopLayoutRect shellFallbackBox;
  DesktopLayoutRect shellRetryButton;
  DesktopLayoutRect shellStartButton;
  DesktopLayoutRect shellStartHostButton;
  DesktopLayoutRect shellOpenHostButton;
};

DesktopLayoutSurfaceMode ResolveDesktopLayoutSurfaceMode(const DesktopLayoutStateInput& input);
DesktopPageVisibility BuildDesktopPageVisibility(const DesktopLayoutStateInput& input);
DesktopLayoutGeometry BuildDesktopLayoutGeometry(int width, int height, DesktopLayoutSurfaceMode surfaceMode);

} // namespace lan::runtime
