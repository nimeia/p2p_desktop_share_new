#include "core/runtime/desktop_shell_presenter.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "desktop shell presenter test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto nav = ResolveDesktopShellCommand(1019);
  Expect(nav.handled, "dashboard nav command should be handled");
  Expect(nav.kind == DesktopShellCommandKind::NavigatePage, "dashboard nav should map to page navigation");
  Expect(nav.page == DesktopShellPage::Dashboard, "dashboard nav should target dashboard page");

  const auto editDraft = ResolveDesktopShellCommand(1200);
  Expect(editDraft.handled, "session edit command should be handled");
  Expect(editDraft.kind == DesktopShellCommandKind::EditSessionDraftChanged,
         "session edit command should refresh edit session state");

  const auto exportBundle = ResolveDesktopShellCommand(1175);
  Expect(exportBundle.handled, "offline export command should be handled");
  Expect(exportBundle.kind == DesktopShellCommandKind::ExecuteHostAction,
         "offline export should reuse host action flow");
  Expect(exportBundle.hostAction == HostActionKind::ExportShareBundle,
         "offline export should route to export share bundle action");

  const auto adapter = ResolveDesktopShellCommand(1163);
  Expect(adapter.handled, "adapter select command should be handled");
  Expect(adapter.kind == DesktopShellCommandKind::SelectNetworkCandidate,
         "adapter button should map to network candidate selection");
  Expect(adapter.index == 2, "third adapter button should map to index 2");

  const auto unknown = ResolveDesktopShellCommand(9999);
  Expect(!unknown.handled, "unknown command should not be handled");

  const auto nativeButtons = BuildNativeCommandButtonPolicy({true, false, true, false});
  Expect(!nativeButtons.startEnabled, "start should be disabled while server is running");
  Expect(nativeButtons.stopEnabled, "stop should be enabled while server is running");
  Expect(nativeButtons.startHotspotEnabled, "start hotspot should be enabled when supported and stopped");
  Expect(!nativeButtons.stopHotspotEnabled, "stop hotspot should be disabled when hotspot is stopped");
  Expect(!nativeButtons.shellStartEnabled, "shell start should respect fallback model state");

  DashboardViewModel dashboard;
  dashboard.primaryActionEnabled = false;
  dashboard.suggestions.push_back({AdminDashboardSuggestionKind::StartServer, L"Fix", L"detail", true, false, true});
  const auto dashboardPolicy = BuildDashboardButtonPolicy(dashboard);
  Expect(!dashboardPolicy.primaryActionEnabled, "dashboard primary action should mirror view model");
  Expect(dashboardPolicy.suggestionFixEnabled[0], "first suggestion fix button should be enabled");
  Expect(!dashboardPolicy.suggestionInfoEnabled[0], "first suggestion info button should mirror view model");
  Expect(dashboardPolicy.suggestionSetupEnabled[0], "first suggestion setup button should mirror view model");
  Expect(!dashboardPolicy.suggestionFixEnabled[1], "unused slots should disable fix button");
  Expect(dashboardPolicy.suggestionInfoEnabled[1], "unused slots should keep info enabled");
  Expect(dashboardPolicy.suggestionSetupEnabled[1], "unused slots should keep setup enabled");

  const auto networkPolicy = BuildNetworkButtonPolicy({true, false, true, false});
  Expect(networkPolicy.adapterSelectEnabled[0], "first adapter slot should be enabled when candidate exists");
  Expect(!networkPolicy.adapterSelectEnabled[1], "second adapter slot should be disabled when empty");
  Expect(networkPolicy.adapterSelectEnabled[2], "third adapter slot should be enabled when candidate exists");

  std::cout << "desktop shell presenter tests passed\n";
  return 0;
}
