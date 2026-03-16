#include "core/runtime/desktop_layout_presenter.h"

#include <algorithm>

namespace lan::runtime {

DesktopLayoutSurfaceMode ResolveDesktopLayoutSurfaceMode(const DesktopLayoutStateInput& input) {
  if (input.preferHtmlAdminUi) {
    return DesktopLayoutSurfaceMode::HtmlAdminPreview;
  }
  if (input.currentPage == DesktopLayoutPage::Setup && input.serverRunning) {
    return DesktopLayoutSurfaceMode::HostPreview;
  }
  return DesktopLayoutSurfaceMode::Hidden;
}

DesktopPageVisibility BuildDesktopPageVisibility(const DesktopLayoutStateInput& input) {
  DesktopPageVisibility visibility;
  if (input.preferHtmlAdminUi) {
    visibility.showNativeNavigation = false;
    return visibility;
  }

  visibility.showNativeNavigation = true;
  switch (input.currentPage) {
    case DesktopLayoutPage::Dashboard:
      visibility.showDashboardPage = true;
      break;
    case DesktopLayoutPage::Setup:
      visibility.showSetupPage = true;
      visibility.showOpenHostButton = true;
      visibility.showHostPreviewPlaceholder = !input.webviewReady;
      break;
    case DesktopLayoutPage::Network:
      visibility.showNetworkPage = true;
      break;
    case DesktopLayoutPage::Sharing:
      visibility.showSharingPage = true;
      break;
    case DesktopLayoutPage::Monitor:
      visibility.showMonitorPage = true;
      break;
    case DesktopLayoutPage::Diagnostics:
      visibility.showDiagnosticsPage = true;
      break;
    case DesktopLayoutPage::Settings:
      visibility.showSettingsPage = true;
      break;
  }

  return visibility;
}

DesktopLayoutGeometry BuildDesktopLayoutGeometry(int width, int height, DesktopLayoutSurfaceMode surfaceMode) {
  DesktopLayoutGeometry geometry;
  const int safeWidth = std::max(width, 0);
  const int safeHeight = std::max(height, 0);
  const int pad = 10;

  switch (surfaceMode) {
    case DesktopLayoutSurfaceMode::HtmlAdminPreview:
      geometry.webview = {0, 0, safeWidth, safeHeight};
      break;
    case DesktopLayoutSurfaceMode::HostPreview:
      geometry.webview = {pad, pad, std::max(safeWidth - pad * 2, 0), std::max(safeHeight - pad * 2, 0)};
      break;
    case DesktopLayoutSurfaceMode::Hidden:
      geometry.webview = {0, 0, 0, 0};
      break;
  }

  const int boxWidth = std::max(320, safeWidth - pad * 6);
  const int boxHeight = std::max(180, safeHeight - 180);
  const int buttonY = 70 + boxHeight + 10;
  geometry.shellFallbackBox = {pad * 2, 70, boxWidth, boxHeight};
  geometry.shellRetryButton = {pad * 2, buttonY, 150, 30};
  geometry.shellStartButton = {pad * 2 + 160, buttonY, 140, 30};
  geometry.shellStartHostButton = {pad * 2 + 310, buttonY, 170, 30};
  geometry.shellOpenHostButton = {pad * 2 + 490, buttonY, 180, 30};
  return geometry;
}

} // namespace lan::runtime
