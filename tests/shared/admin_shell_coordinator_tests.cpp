#include "core/runtime/admin_shell_coordinator.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "admin shell coordinator test failed: " << message << "\n";
    std::exit(1);
  }
}

lan::runtime::ShellBridgeInboundMessage BuildAdminCommand(lan::runtime::ShellBridgeAdminCommandKind kind) {
  lan::runtime::ShellBridgeInboundMessage message;
  message.source = lan::runtime::ShellBridgeSource::AdminShell;
  message.kind = lan::runtime::ShellBridgeInboundKind::AdminCommand;
  message.adminCommand.kind = kind;
  return message;
}

} // namespace

int main() {
  using namespace lan::runtime;

  int refreshCalls = 0;
  int generateCalls = 0;
  int applySessionCalls = 0;
  int applyHotspotCalls = 0;
  int hostActionCalls = 0;
  int copyViewerCalls = 0;
  int quickFixCalls = 0;
  int selectAdapterCalls = 0;
  int startHotspotCalls = 0;
  int stopHotspotCalls = 0;
  int autoHotspotCalls = 0;
  int hotspotSettingsCalls = 0;
  int firewallSettingsCalls = 0;
  int networkDiagnosticsCalls = 0;
  int webviewRuntimeCalls = 0;
  int trustCertificateCalls = 0;
  int remoteProbeGuideCalls = 0;
  int connectedDevicesCalls = 0;
  int navigateCalls = 0;
  HostActionKind lastAction = HostActionKind::StartServer;
  AdminShellSessionRequest sessionRequest;
  AdminShellHotspotRequest hotspotRequest;
  std::size_t selectedIndex = 0;
  std::wstring navigatedPage;

  AdminShellCoordinatorHooks hooks;
  hooks.refreshRuntime = [&]() { ++refreshCalls; };
  hooks.generateRoomToken = [&]() { ++generateCalls; };
  hooks.applySessionConfig = [&](const AdminShellSessionRequest& request) {
    ++applySessionCalls;
    sessionRequest = request;
  };
  hooks.applyHotspotConfig = [&](const AdminShellHotspotRequest& request) {
    ++applyHotspotCalls;
    hotspotRequest = request;
  };
  hooks.executeHostAction = [&](HostActionKind action) {
    ++hostActionCalls;
    lastAction = action;
  };
  hooks.copyViewerUrl = [&]() { ++copyViewerCalls; };
  hooks.quickFixNetwork = [&]() { ++quickFixCalls; };
  hooks.selectNetworkCandidate = [&](std::size_t index) {
    ++selectAdapterCalls;
    selectedIndex = index;
  };
  hooks.startHotspot = [&]() { ++startHotspotCalls; };
  hooks.stopHotspot = [&]() { ++stopHotspotCalls; };
  hooks.autoHotspot = [&]() { ++autoHotspotCalls; };
  hooks.openHotspotSettings = [&]() { ++hotspotSettingsCalls; };
  hooks.openFirewallSettings = [&]() { ++firewallSettingsCalls; };
  hooks.runNetworkDiagnostics = [&]() { ++networkDiagnosticsCalls; };
  hooks.checkWebViewRuntime = [&]() { ++webviewRuntimeCalls; };
  hooks.trustLocalCertificate = [&]() { ++trustCertificateCalls; };
  hooks.exportRemoteProbeGuide = [&]() { ++remoteProbeGuideCalls; };
  hooks.openConnectedDevices = [&]() { ++connectedDevicesCalls; };
  hooks.navigatePage = [&](std::wstring page) {
    ++navigateCalls;
    navigatedPage = std::move(page);
  };

  auto ready = ShellBridgeInboundMessage{};
  ready.source = ShellBridgeSource::AdminShell;
  ready.kind = ShellBridgeInboundKind::AdminReady;
  auto readyResult = HandleAdminShellMessage(ready, hooks);
  Expect(readyResult.requestSnapshot, "admin ready should request snapshot");

  auto refresh = BuildAdminCommand(ShellBridgeAdminCommandKind::RefreshNetwork);
  auto refreshResult = HandleAdminShellMessage(refresh, hooks);
  Expect(refreshResult.stateChanged, "refresh should mark state changed");
  Expect(refreshCalls == 1, "refresh hook should run");

  auto applySession = BuildAdminCommand(ShellBridgeAdminCommandKind::ApplySession);
  applySession.adminCommand.room = L"alpha";
  applySession.adminCommand.token = L"beta";
  applySession.adminCommand.bind = L"0.0.0.0";
  applySession.adminCommand.port = 9555;
  HandleAdminShellMessage(applySession, hooks);
  Expect(applySessionCalls == 1, "apply session hook should run");
  Expect(sessionRequest.room == L"alpha", "apply session should preserve room");
  Expect(sessionRequest.token == L"beta", "apply session should preserve token");
  Expect(sessionRequest.bindAddress == L"0.0.0.0", "apply session should preserve bind");
  Expect(sessionRequest.port == 9555, "apply session should preserve port");

  auto openHost = BuildAdminCommand(ShellBridgeAdminCommandKind::OpenHost);
  HandleAdminShellMessage(openHost, hooks);
  Expect(hostActionCalls == 1, "host action hook should run for open host");
  Expect(lastAction == HostActionKind::OpenHostPage, "open host should map to host action");

  auto copyViewer = BuildAdminCommand(ShellBridgeAdminCommandKind::CopyViewerUrl);
  HandleAdminShellMessage(copyViewer, hooks);
  Expect(copyViewerCalls == 1, "copy viewer hook should run");

  auto quickFix = BuildAdminCommand(ShellBridgeAdminCommandKind::QuickFixNetwork);
  HandleAdminShellMessage(quickFix, hooks);
  Expect(quickFixCalls == 1, "quick fix hook should run");

  auto selectAdapter = BuildAdminCommand(ShellBridgeAdminCommandKind::SelectAdapter);
  selectAdapter.adminCommand.hasIndex = true;
  selectAdapter.adminCommand.index = 3;
  HandleAdminShellMessage(selectAdapter, hooks);
  Expect(selectAdapterCalls == 1, "select adapter hook should run");
  Expect(selectedIndex == 3, "selected adapter index should be preserved");

  auto applyHotspot = BuildAdminCommand(ShellBridgeAdminCommandKind::ApplyHotspot);
  applyHotspot.adminCommand.ssid = L"LanScreenShare";
  applyHotspot.adminCommand.password = L"12345678";
  HandleAdminShellMessage(applyHotspot, hooks);
  Expect(applyHotspotCalls == 1, "apply hotspot hook should run");
  Expect(hotspotRequest.ssid == L"LanScreenShare", "hotspot ssid should be preserved");
  Expect(hotspotRequest.password == L"12345678", "hotspot password should be preserved");

  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::StartHotspot), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::StopHotspot), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::AutoHotspot), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::OpenHotspotSettings), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::OpenFirewallSettings), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::RunNetworkDiagnostics), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::ExportRemoteProbeGuide), hooks);
  HandleAdminShellMessage(BuildAdminCommand(ShellBridgeAdminCommandKind::OpenConnectedDevices), hooks);
  Expect(startHotspotCalls == 1, "start hotspot hook should run");
  Expect(stopHotspotCalls == 1, "stop hotspot hook should run");
  Expect(autoHotspotCalls == 1, "auto hotspot hook should run");
  Expect(hotspotSettingsCalls == 1, "open hotspot settings hook should run");
  Expect(firewallSettingsCalls == 1, "open firewall settings hook should run");
  Expect(networkDiagnosticsCalls == 1, "run network diagnostics hook should run");
  Expect(remoteProbeGuideCalls == 1, "export remote probe guide hook should run");
  Expect(connectedDevicesCalls == 1, "open connected devices hook should run");

  auto switchPage = BuildAdminCommand(ShellBridgeAdminCommandKind::SwitchPage);
  switchPage.adminCommand.page = L"network";
  HandleAdminShellMessage(switchPage, hooks);
  Expect(navigateCalls == 1, "navigate hook should run");
  Expect(navigatedPage == L"network", "navigate page should be preserved");

  auto unknown = BuildAdminCommand(ShellBridgeAdminCommandKind::None);
  unknown.adminCommand.commandName = L"mystery";
  auto unknownResult = HandleAdminShellMessage(unknown, hooks);
  Expect(!unknownResult.stateChanged, "unknown command should not mark state changed");
  Expect(unknownResult.logLine.find(L"mystery") != std::wstring::npos, "unknown command should mention name");

  auto ignored = ShellBridgeInboundMessage{};
  ignored.source = ShellBridgeSource::HostPage;
  ignored.kind = ShellBridgeInboundKind::HostLog;
  ignored.rawPayload = L"{\"kind\":\"log\"}";
  auto ignoredResult = HandleAdminShellMessage(ignored, hooks);
  Expect(!ignoredResult.requestSnapshot, "host page message should not request snapshot");
  Expect(ignoredResult.logLine.find(L"ignored") != std::wstring::npos, "host page message should be ignored");

  std::cout << "admin shell coordinator tests passed\n";
  return 0;
}
