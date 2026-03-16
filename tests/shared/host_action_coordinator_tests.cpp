#include "core/runtime/host_action_coordinator.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "host action coordinator test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  int startCalls = 0;
  int stopCalls = 0;
  int openHostCalls = 0;
  int openViewerCalls = 0;
  int ensureCalls = 0;
  int openPathCalls = 0;

  HostActionHooks hooks;
  hooks.startServer = [&]() {
    ++startCalls;
    return HostActionOperation{true, true, L""};
  };
  hooks.stopServer = [&]() {
    ++stopCalls;
    return HostActionOperation{true, true, L""};
  };
  hooks.openHostPage = [&]() {
    ++openHostCalls;
    return HostActionOperation{true, true, L"Opened Host page in browser"};
  };
  hooks.openViewerPage = [&]() {
    ++openViewerCalls;
    return HostActionOperation{true, true, L"Opened Viewer page in browser"};
  };
  hooks.ensureArtifacts = [&](const HostActionArtifactRequest& request, HostActionArtifactPaths& paths) {
    ++ensureCalls;
    if (request.shareCard) paths.shareCardPath = std::filesystem::path("out") / "share_card.html";
    if (request.shareWizard) paths.shareWizardPath = std::filesystem::path("out") / "share_wizard.html";
    if (request.bundleJson) paths.bundleJsonPath = std::filesystem::path("out") / "share_bundle.json";
    if (request.desktopSelfCheck) paths.desktopSelfCheckPath = std::filesystem::path("out") / "desktop_self_check.html";
    return HostActionOperation{true, true, L""};
  };
  hooks.openPath = [&](const std::filesystem::path& path) {
    ++openPathCalls;
    return HostActionOperation{true, true, L"Opened: " + path.wstring()};
  };

  HostActionContext context;
  context.outputDir = std::filesystem::path("out") / "share_bundle";
  context.diagnosticsReportPath = context.outputDir / "share_diagnostics.txt";
  context.diagnosticsReportExists = false;

  auto startAndOpen = ExecuteHostAction(HostActionKind::StartAndOpenHost, context, hooks);
  Expect(startAndOpen.ok, "start and open host should succeed");
  Expect(startCalls == 1, "start hook should run once");
  Expect(openHostCalls == 1, "open host hook should run once");
  Expect(startAndOpen.timelineEvents.size() == 2, "start and open should produce two timeline entries");
  Expect(startAndOpen.effects.refreshShareInfo, "start should request share info refresh");

  auto qr = ExecuteHostAction(HostActionKind::ShowQr, context, hooks);
  Expect(qr.ok, "show qr should succeed");
  Expect(qr.effects.handoffStarted, "show qr should mark handoff started");
  Expect(qr.effects.shareCardExported, "show qr should mark share card exported");
  Expect(ensureCalls == 1, "show qr should ensure artifacts once");
  Expect(openPathCalls == 1, "show qr should open generated file");
  Expect(qr.paths.shareCardPath.filename() == "share_card.html", "share qr should populate share card path");

  auto diag = ExecuteHostAction(HostActionKind::OpenDiagnosticsReport, context, hooks);
  Expect(diag.ok, "open diagnostics should succeed when export can regenerate report");
  Expect(ensureCalls == 2, "open diagnostics should regenerate artifacts when report missing");
  Expect(openPathCalls == 2, "open diagnostics should open report path");

  hooks.startServer = [&]() {
    ++startCalls;
    return HostActionOperation{false, true, L"Start failed: port busy"};
  };
  auto fail = ExecuteHostAction(HostActionKind::StartServer, context, hooks);
  Expect(!fail.ok, "start failure should bubble up");
  Expect(!fail.logs.empty(), "start failure should log details");
  Expect(fail.logs.front().find(L"port busy") != std::wstring::npos, "start failure should preserve error text");

  std::cout << "host action coordinator tests passed\n";
  return 0;
}
