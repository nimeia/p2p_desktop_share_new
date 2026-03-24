#include "core/runtime/diagnostics_view_model_assembler.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "diagnostics view model assembler test failed: " << message << "\n";
    std::exit(1);
  }
}

lan::runtime::AdminViewModelInput MakeInput() {
  using namespace lan::runtime;
  AdminViewModelInput input;
  input.sessionModel.bindAddress = L"0.0.0.0";
  input.sessionModel.port = 9443;
  input.sessionModel.room = L"ROOM01";
  input.sessionModel.token = L"TOKEN01";
  input.runtimeSnapshot.session.hostIp = L"192.168.1.10";
  input.runtimeSnapshot.session.bindAddress = L"0.0.0.0";
  input.runtimeSnapshot.session.port = 9443;
  input.runtimeSnapshot.session.room = L"ROOM01";
  input.runtimeSnapshot.session.token = L"TOKEN01";
  input.runtimeSnapshot.session.hostPageState = L"sharing";
  input.runtimeSnapshot.session.hotspotStatus = L"running";
  input.runtimeSnapshot.session.webviewStatusText = L"ready";
  input.runtimeSnapshot.session.shareCardExported = true;
  input.runtimeSnapshot.session.lastRooms = 2;
  input.runtimeSnapshot.session.lastViewers = 3;
  input.runtimeSnapshot.health.serverProcessRunning = true;
  input.runtimeSnapshot.health.portReady = true;
  input.runtimeSnapshot.health.localHealthReady = true;
  input.runtimeSnapshot.health.lanBindReady = true;
  input.runtimeSnapshot.health.embeddedHostReady = false;
  input.runtimeSnapshot.health.hostIpReachable = true;
  input.runtimeSnapshot.health.firewallReady = false;
  input.runtimeSnapshot.health.firewallDetail = L"No inbound allow rule";
  input.bundleDir = L"C:/app/out/share_bundle";
  input.timelineText = L"Started -> Sharing";
  input.logTail = L"latest merged logs";
  return input;
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto input = MakeInput();
  const auto monitor = BuildMonitorViewModel(input);
  Expect(monitor.metricCards[0].find(L"Rooms\n2") != std::wstring::npos,
         "monitor metrics should include room count");
  Expect(monitor.timelineText == L"Started -> Sharing", "monitor timeline should preserve timeline text");
  Expect(monitor.detailText.find(L"latest merged logs") != std::wstring::npos,
         "monitor detail should include log tail");

  const auto diagnostics = BuildDiagnosticsViewModel(input);
  Expect(diagnostics.checklistCard.find(L"[OK] Plain HTTP mode") != std::wstring::npos,
         "diagnostics checklist should reflect the current HTTP transport mode");
  Expect(diagnostics.checklistCard.find(L"Firewall inbound path") != std::wstring::npos,
         "diagnostics checklist should include firewall line");
  Expect(diagnostics.checklistCard.find(L"No inbound allow rule") != std::wstring::npos,
         "diagnostics checklist should include firewall detail");
  Expect(diagnostics.exportCard.find(L"C:/app/out/share_bundle") != std::wstring::npos,
         "diagnostics export card should include bundle dir");
  Expect(diagnostics.filesCard.find(L"share_diagnostics.txt") != std::wstring::npos,
         "diagnostics files card should list exported files");

  ShellStateInput shell;
  shell.htmlAdminMode = true;
  shell.adminShellReady = false;
  shell.serverRunning = false;
  shell.uiBundleExists = false;
  shell.webviewStatus = L"runtime-unavailable";
  shell.webviewDetail = L"runtime missing";
  const auto fallback = BuildShellFallbackViewModel(shell);
  Expect(fallback.showFallback, "shell fallback should be visible when html shell is unavailable");
  Expect(fallback.bodyText.find(L"Install or repair Microsoft WebView2 Runtime") != std::wstring::npos,
         "shell fallback should include runtime recovery guidance");
  Expect(fallback.startButtonLabel == L"Start Service", "shell fallback should expose start label");
  Expect(fallback.startButtonEnabled, "shell fallback start button should be enabled when server stopped");

  shell.webviewStatus = L"not-initialized";
  shell.webviewDetail.clear();
  shell.shellStartupError = L"Start failed: server exe not found";
  const auto startupFallback = BuildShellFallbackViewModel(shell);
  Expect(startupFallback.showFallback, "shell fallback should stay visible when admin startup fails");
  Expect(startupFallback.bodyText.find(L"server exe not found") != std::wstring::npos,
         "shell fallback should surface startup failure detail");

  shell.shellStartupError.clear();
  shell.uiBundleExists = true;
  shell.webviewStatus = L"ready";
  const auto loadingShell = BuildShellFallbackViewModel(shell);
  Expect(!loadingShell.showFallback,
         "shell fallback should not hide a ready WebView while the admin shell waits for its first snapshot");

  std::cout << "diagnostics view model assembler tests passed\n";
  return 0;
}
