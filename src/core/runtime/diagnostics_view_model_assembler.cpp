#include "core/runtime/diagnostics_view_model_assembler.h"

#include "core/runtime/network_diagnostics_policy.h"

#include <sstream>

namespace lan::runtime {
namespace {

std::wstring DisplayOrFallback(std::wstring_view value, std::wstring_view fallback) {
  return value.empty() ? std::wstring(fallback) : std::wstring(value);
}

} // namespace

MonitorViewModel BuildMonitorViewModel(const AdminViewModelInput& input) {
  const auto& runtime = input.runtimeSnapshot;
  const auto& session = runtime.session;
  const auto& health = runtime.health;

  MonitorViewModel model;
  model.metricCards = {
      std::wstring(L"Rooms\n") + std::to_wstring(session.lastRooms),
      std::wstring(L"Viewers\n") + std::to_wstring(session.lastViewers),
      std::wstring(L"Host State\n") + DisplayOrFallback(session.hostPageState, L"unknown"),
      std::wstring(L"/health\n") + (health.localHealthReady ? L"OK" : L"ATTN"),
      std::wstring(L"Latency\n") + (health.localHealthReady ? L"<1s" : L"n/a"),
  };
  model.timelineText = input.timelineText.empty() ? L"No timeline events yet." : input.timelineText;

  std::wstringstream detail;
  detail << L"Health Checks\r\n";
  detail << L"- Local /health: " << (health.localHealthReady ? L"green / normal" : L"red / abnormal") << L"\r\n";
  detail << L"- Host reachability: " << (health.hostIpReachable ? L"green / normal" : L"yellow / attention") << L"\r\n";
  detail << L"- WebView: " << (health.embeddedHostReady ? L"green / normal" : L"gray / inactive") << L"\r\n\r\n";
  detail << L"Connection Events\r\n";
  detail << L"- Rooms: " << session.lastRooms << L"\r\n";
  detail << L"- Viewers: " << session.lastViewers << L"\r\n\r\n";
  detail << L"Realtime Logs\r\n";
  detail << L"- Info / Warning / Error / WebView / Server stdout-stderr are currently merged below.\r\n\r\n";
  detail << input.logTail;
  model.detailText = detail.str();
  return model;
}

DiagnosticsViewModel BuildDiagnosticsViewModel(const AdminViewModelInput& input) {
  const auto& runtime = input.runtimeSnapshot;
  const auto& health = runtime.health;
  const auto& session = runtime.session;
  const auto networkDiagnostics = BuildNetworkDiagnosticsViewModel(session, health);

  DiagnosticsViewModel model;

  std::wstringstream checklist;
  checklist << L"[" << (health.portReady ? L"OK" : L"FIX") << L"] Port listening normal\r\n";
  checklist << L"  Fix: free the port or change bind/port if needed.\r\n\r\n";
  checklist << L"[" << (health.localHealthReady ? L"OK" : L"FIX") << L"] Local Host opens\r\n";
  checklist << L"  Fix: start service, then test Host URL in browser.\r\n\r\n";
  checklist << L"[" << (health.embeddedHostReady ? L"OK" : L"FIX") << L"] WebView2 available\r\n";
  checklist << L"  Fix: install/repair WebView2 Runtime or use browser fallback.\r\n\r\n";
  checklist << L"[OK] Plain HTTP mode\r\n";
  checklist << L"  Fix: no certificate trust step is required in this mode.\r\n";
  if (!health.certDetail.empty()) {
    checklist << L"  Detail: " << health.certDetail << L"\r\n";
  }
  checklist << L"\r\n";
  checklist << L"[" << (!session.hostIp.empty() ? L"OK" : L"FIX") << L"] LAN IP determined\r\n";
  checklist << L"  Fix: re-detect network or choose main IP manually.\r\n\r\n";
  checklist << L"[" << (networkDiagnostics.firewallReady ? L"OK" : L"FIX") << L"] Firewall inbound path\r\n";
  checklist << L"  Fix: open Windows Firewall settings or add an inbound allow rule for the server executable / port.\r\n";
  if (!networkDiagnostics.firewallDetail.empty()) {
    checklist << L"  Detail: " << networkDiagnostics.firewallDetail << L"\r\n";
  }
  checklist << L"\r\n";
  checklist << L"[" << (networkDiagnostics.remoteViewerReady ? L"OK" : L"FIX") << L"] Remote viewer reachability\r\n";
  checklist << L"  Fix: keep the viewer on the same LAN / hotspot and validate the Viewer URL from another device.\r\n";
  if (!networkDiagnostics.remoteViewerDetail.empty()) {
    checklist << L"  Detail: " << networkDiagnostics.remoteViewerDetail << L"\r\n";
  }
  checklist << L"\r\n";
  checklist << L"[" << (session.shareCardExported ? L"OK" : L"FIX") << L"] Share bundle exported\r\n";
  checklist << L"  Fix: export bundle or open Sharing Center.";
  model.checklistCard = checklist.str();

  std::wstringstream actions;
  actions << L"Operator Actions\r\n\r\n";
  actions << L"1. Confirm guest and host are on the same subnet.\r\n";
  actions << L"2. If Viewer fails, paste the Viewer URL directly into a browser.\r\n";
  actions << L"3. If remote viewers cannot connect, verify the firewall and selected adapter path before regenerating the bundle.\r\n";
  actions << L"4. If hotspot fails, open system hotspot settings and start it manually.\r\n";
  actions << L"5. If host preview is unavailable, open Host in the system browser.\r\n\r\n";
  actions << L"Current Network Diagnostics\r\n";
  actions << L"- Firewall: " << networkDiagnostics.firewallLabel << L"\r\n";
  actions << L"  " << networkDiagnostics.firewallAction << L"\r\n";
  actions << L"- Remote Viewer Path: " << networkDiagnostics.remoteViewerLabel << L"\r\n";
  actions << L"  " << networkDiagnostics.remoteViewerAction;
  model.actionsCard = actions.str();

  std::wstringstream exportCard;
  exportCard << L"Export Actions\r\n\r\n";
  exportCard << L"Output Dir\r\n" << input.bundleDir << L"\r\n\r\n";
  exportCard << L"Use buttons on the right to open, copy, refresh, and export.";
  model.exportCard = exportCard.str();

  model.filesCard =
      L"Exported Files\r\n\r\nshare_card.html\r\nshare_wizard.html\r\nshare_bundle.json\r\n"
      L"share_status.js\r\nshare_diagnostics.txt\r\ndesktop_self_check.html\r\ndesktop_self_check.txt";

  return model;
}

ShellFallbackViewModel BuildShellFallbackViewModel(const ShellStateInput& input) {
  ShellFallbackViewModel model;
  model.showFallback = input.htmlAdminMode && (!input.adminShellReady || input.webviewStatus != L"ready");
  model.startButtonLabel = input.serverRunning ? L"Service Running" : L"Start Service";
  model.startButtonEnabled = !input.serverRunning;
  model.startHostButtonLabel = input.serverRunning ? L"Open Host In Browser" : L"Start + Open Host";

  if (!model.showFallback) {
    return model;
  }

  std::wstringstream body;
  body << L"HTML admin shell is not ready yet.\r\n\r\n";
  body << L"WebView status: " << DisplayOrFallback(input.webviewStatus, L"unknown") << L"\r\n";
  if (!input.webviewDetail.empty()) {
    body << L"WebView detail: " << input.webviewDetail << L"\r\n";
  }
  body << L"Admin shell ready: " << (input.adminShellReady ? L"yes" : L"no") << L"\r\n";
  body << L"Server running: " << (input.serverRunning ? L"yes" : L"no") << L"\r\n";
  body << L"UI bundle found: " << (input.uiBundleExists ? L"yes" : L"no") << L"\r\n\r\n";

  if (!input.shellStartupError.empty()) {
    body << L"Reason\r\n";
    body << L"- The local admin server failed before the HTML shell could finish loading.\r\n";
    body << L"- Detail: " << input.shellStartupError << L"\r\n\r\n";
    body << L"Next step\r\n";
    body << L"- Confirm lan_screenshare_server.exe, www, and webui are present beside this build, then click Retry Loading UI.\r\n";
    body << L"- If the detail mentions a bind or port failure, free the port or change the configured bind/port before retrying.\r\n";
    body << L"- You can still use Start Service / Start + Open Host below.";
  } else if (input.webviewStatus == L"sdk-unavailable") {
    body << L"Reason\r\n";
    body << L"- This build was compiled without WebView2 SDK headers, so the HTML admin cannot be embedded.\r\n\r\n";
    body << L"Next step\r\n";
    body << L"- Restore desktop NuGet packages and rebuild the app, then click Retry Loading UI.\r\n";
    body << L"- You can still use Start Service / Start + Open Host below.";
  } else if (input.webviewStatus == L"controller-unavailable" &&
             input.webviewDetail.find(L"0x8007139F") != std::wstring::npos) {
    body << L"Reason\r\n";
    body << L"- WebView2 Runtime is installed, but controller creation returned INVALID_STATE (0x8007139F).\r\n";
    body << L"- This usually means the embedded browser was started in a bad runtime state or from an older build.\r\n\r\n";
    body << L"Next step\r\n";
    body << L"- Close all LanScreenShareHostApp processes and relaunch the newest Debug build.\r\n";
    body << L"- Click Retry Loading UI once after relaunch.\r\n";
    body << L"- If it still fails, keep the detail line and check the latest desktop log.\r\n";
    body << L"- You can still use Start Service / Start + Open Host below.";
  } else if (input.webviewStatus == L"runtime-unavailable" || input.webviewStatus == L"controller-unavailable") {
    body << L"Reason\r\n";
    body << L"- WebView2 is unavailable on this machine, so the HTML admin cannot be embedded.\r\n\r\n";
    body << L"Next step\r\n";
    body << L"- Install or repair Microsoft WebView2 Runtime, then click Retry Loading UI.\r\n";
    body << L"- You can still use Start Service / Start + Open Host below.";
  } else if (!input.uiBundleExists) {
    body << L"Reason\r\n";
    body << L"- The desktop webui bundle is missing from the runtime directory.\r\n\r\n";
    body << L"Next step\r\n";
    body << L"- Rebuild or copy the webui output, then click Retry Loading UI.\r\n";
    body << L"- You can still use Start Service / Start + Open Host below.";
  } else {
    body << L"Reason\r\n";
    body << L"- WebView2 is still initializing or the HTML shell has not replied with a ready snapshot yet.\r\n\r\n";
    body << L"Next step\r\n";
    body << L"- Wait a moment, or click Retry Loading UI.\r\n";
    body << L"- If it keeps failing, use Start Service / Start + Open Host below and inspect logs.";
  }

  model.bodyText = body.str();
  return model;
}

} // namespace lan::runtime
