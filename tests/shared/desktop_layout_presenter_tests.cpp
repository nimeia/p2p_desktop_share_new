#include "core/runtime/desktop_layout_presenter.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "desktop layout presenter test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  DesktopLayoutStateInput input;
  input.currentPage = DesktopLayoutPage::Setup;
  input.serverRunning = true;
  input.webviewReady = false;
  auto surface = ResolveDesktopLayoutSurfaceMode(input);
  Expect(surface == DesktopLayoutSurfaceMode::HostPreview, "setup while running should embed host preview");

  auto visibility = BuildDesktopPageVisibility(input);
  Expect(visibility.showNativeNavigation, "native navigation should be visible in native shell mode");
  Expect(visibility.showSetupPage, "setup page should be visible");
  Expect(visibility.showHostPreviewPlaceholder, "setup page should show placeholder when preview is unavailable");
  Expect(visibility.showOpenHostButton, "setup page should expose open-host button");
  Expect(!visibility.showDashboardPage, "dashboard page should remain hidden");

  input.preferHtmlAdminUi = true;
  surface = ResolveDesktopLayoutSurfaceMode(input);
  Expect(surface == DesktopLayoutSurfaceMode::HtmlAdminPreview, "html admin preference should force html-admin surface");
  visibility = BuildDesktopPageVisibility(input);
  Expect(!visibility.showNativeNavigation, "native navigation should hide in html-admin mode");
  Expect(!visibility.showSetupPage, "native setup page should hide in html-admin mode");

  const auto geometry = BuildDesktopLayoutGeometry(1280, 720, DesktopLayoutSurfaceMode::HostPreview);
  Expect(geometry.webview.x == 10 && geometry.webview.y == 10, "host preview should be inset by padding");
  Expect(geometry.webview.width == 1260 && geometry.webview.height == 700, "host preview should fill client area minus padding");
  Expect(geometry.shellFallbackBox.x == 20 && geometry.shellFallbackBox.y == 70, "shell fallback box should anchor near the top-left");
  Expect(geometry.shellRetryButton.width == 150, "retry button width should stay stable");

  const auto hiddenGeometry = BuildDesktopLayoutGeometry(800, 600, DesktopLayoutSurfaceMode::Hidden);
  Expect(hiddenGeometry.webview.width == 0 && hiddenGeometry.webview.height == 0, "hidden surface should collapse the webview");

  std::cout << "desktop layout presenter tests passed\n";
  return 0;
}
