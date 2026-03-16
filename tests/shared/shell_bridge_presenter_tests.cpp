#include "core/runtime/shell_bridge_presenter.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "shell bridge presenter test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto ready = ParseShellBridgeInboundMessage(L"{\"source\":\"admin-shell\",\"kind\":\"ready\"}");
  Expect(ready.source == ShellBridgeSource::AdminShell, "admin shell source should be parsed");
  Expect(ready.kind == ShellBridgeInboundKind::AdminReady, "ready kind should map correctly");

  const auto apply = ParseShellBridgeInboundMessage(
      L"{\"source\":\"admin-shell\",\"kind\":\"command\",\"command\":\"apply-session\",\"room\":\"alpha\",\"token\":\"beta\",\"bind\":\"0.0.0.0\",\"port\":9444}");
  Expect(apply.kind == ShellBridgeInboundKind::AdminCommand, "admin command should be parsed");
  Expect(apply.adminCommand.kind == ShellBridgeAdminCommandKind::ApplySession, "apply-session should map correctly");
  Expect(apply.adminCommand.room == L"alpha", "room should be parsed");
  Expect(apply.adminCommand.token == L"beta", "token should be parsed");
  Expect(apply.adminCommand.bind == L"0.0.0.0", "bind should be parsed");
  Expect(apply.adminCommand.port == 9444, "port should be parsed");

  const auto adapter = ParseShellBridgeInboundMessage(
      L"{\"source\":\"admin-shell\",\"kind\":\"command\",\"command\":\"select-adapter\",\"index\":2}");
  Expect(adapter.adminCommand.kind == ShellBridgeAdminCommandKind::SelectAdapter, "select-adapter should map correctly");
  Expect(adapter.adminCommand.hasIndex, "adapter index should be marked present");
  Expect(adapter.adminCommand.index == 2, "adapter index should be parsed");

  const auto firewallCommand = ParseShellBridgeInboundMessage(L"{\"source\":\"admin-shell\",\"kind\":\"command\",\"command\":\"open-firewall-settings\"}");
  Expect(firewallCommand.adminCommand.kind == ShellBridgeAdminCommandKind::OpenFirewallSettings, "open-firewall-settings should map correctly");

  const auto diagnosticsCommand = ParseShellBridgeInboundMessage(L"{\"source\":\"admin-shell\",\"kind\":\"command\",\"command\":\"run-network-diagnostics\"}");
  Expect(diagnosticsCommand.adminCommand.kind == ShellBridgeAdminCommandKind::RunNetworkDiagnostics, "run-network-diagnostics should map correctly");

  const auto webviewRuntimeCommand = ParseShellBridgeInboundMessage(L"{\"source\":\"admin-shell\",\"kind\":\"command\",\"command\":\"check-webview-runtime\"}");
  Expect(webviewRuntimeCommand.adminCommand.kind == ShellBridgeAdminCommandKind::CheckWebViewRuntime, "check-webview-runtime should map correctly");

  const auto trustCertCommand = ParseShellBridgeInboundMessage(L"{\"source\":\"admin-shell\",\"kind\":\"command\",\"command\":\"trust-local-certificate\"}");
  Expect(trustCertCommand.adminCommand.kind == ShellBridgeAdminCommandKind::TrustLocalCertificate, "trust-local-certificate should map correctly");

  const auto status = ParseShellBridgeInboundMessage(L"{\"kind\":\"status\",\"state\":\"sharing\",\"viewers\":3}");
  Expect(status.source == ShellBridgeSource::HostPage, "host page source should default correctly");
  Expect(status.kind == ShellBridgeInboundKind::HostStatus, "host status should map correctly");
  Expect(status.hostState == L"sharing", "host state should be parsed");
  Expect(status.hasViewers && status.viewers == 3, "viewer count should be parsed");

  const auto log = ParseShellBridgeInboundMessage(L"{\"kind\":\"log\",\"message\":\"viewer connected\"}");
  Expect(log.kind == ShellBridgeInboundKind::HostLog, "host log should map correctly");
  Expect(log.logMessage == L"viewer connected", "host log message should be parsed");

  ShellBridgeSnapshotState snapshot;
  snapshot.appName = L"LanScreenShareHostApp";
  snapshot.nativePage = L"dashboard";
  snapshot.dashboardState = L"ready";
  snapshot.dashboardLabel = L"Ready";
  snapshot.hostIp = L"192.168.1.5";
  snapshot.hostUrl = L"https://192.168.1.5:9443/host";
  snapshot.viewerUrl = L"https://192.168.1.5:9443/viewer";
  snapshot.serverRunning = true;
  snapshot.firewallReady = false;
  snapshot.firewallDetail = L"Firewall rule missing";
  snapshot.remoteViewerReady = false;
  snapshot.remoteViewerDetail = L"Viewer path blocked";
  snapshot.remoteProbeLabel = L"Recommended adapter answered";
  snapshot.remoteProbeAction = L"Switch to the recommended adapter";
  snapshot.rooms = 1;
  snapshot.viewers = 2;
  snapshot.networkCandidates.push_back({L"Wi-Fi", L"192.168.1.5", L"wireless", true, true, true, L"LAN /health ok", L"Primary adapter answered"});

  const auto json = BuildShellBridgeSnapshotEventJson(snapshot);
  Expect(json.find(L"\"type\":\"event\"") != std::wstring::npos, "snapshot json should contain event envelope");
  Expect(json.find(L"\"name\":\"state.snapshot\"") != std::wstring::npos, "snapshot json should contain event name");
  Expect(json.find(L"\"hostIp\":\"192.168.1.5\"") != std::wstring::npos, "snapshot json should contain host ip");
  Expect(json.find(L"\"networkCandidates\":[{") != std::wstring::npos, "snapshot json should contain network candidate array");
  Expect(json.find(L"\"recommended\":true") != std::wstring::npos, "snapshot json should contain candidate flags");
  Expect(json.find(L"\"firewallReady\":false") != std::wstring::npos, "snapshot json should contain firewall readiness");
  Expect(json.find(L"\"remoteViewerDetail\":\"Viewer path blocked\"") != std::wstring::npos, "snapshot json should contain remote viewer detail");

  std::cout << "shell bridge presenter tests passed\n";
  return 0;
}
