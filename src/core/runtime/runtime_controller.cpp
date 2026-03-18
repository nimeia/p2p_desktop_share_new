#include "core/runtime/runtime_controller.h"
#include "core/i18n/localization.h"

#include <sstream>

namespace lan::runtime {
namespace {

bool IsHostStateSharing(std::wstring_view state) {
  return state == L"sharing" || state == L"shared" || state == L"streaming";
}

bool IsHostStateReadyOrLoading(std::wstring_view state) {
  return state == L"ready" || state == L"loading" || IsHostStateSharing(state);
}

std::wstring HostOrLoopback(const RuntimeSessionState& session) {
  (void)session;
  return L"127.0.0.1";
}

std::wstring ViewerHostOrLoopback(const RuntimeSessionState& session) {
  return session.hostIp.empty() ? L"127.0.0.1" : session.hostIp;
}

} // namespace

std::wstring BuildHostUrl(const RuntimeSessionState& session) {
  std::wstringstream ss;
  ss << L"http://" << HostOrLoopback(session) << L":" << session.port
     << L"/host?room=" << session.room << L"&token=" << session.token;
  return lan::i18n::AppendLocaleQuery(ss.str(), session.localeCode);
}

std::wstring BuildViewerUrl(const RuntimeSessionState& session) {
  std::wstringstream ss;
  ss << L"http://" << ViewerHostOrLoopback(session) << L":" << session.port
     << L"/view?room=" << session.room << L"&token=" << session.token;
  return lan::i18n::AppendLocaleQuery(ss.str(), session.localeCode);
}

RuntimeHandoffSummary BuildHandoffSummary(const RuntimeSessionState& session,
                                          const RuntimeHealthState& health) {
  RuntimeHandoffSummary summary;
  summary.state = L"not-started";
  summary.label = L"Not started";
  summary.detail = L"Open Share Wizard or copy the Viewer URL when you are ready to hand off the session.";

  if (session.lastViewers > 0 || session.handoffDelivered) {
    summary.state = L"delivered";
    summary.label = L"Delivered";
    summary.detail = L"A viewer is connected. Keep sharing, or open diagnostics if the remote device still reports issues.";
    return summary;
  }

  if (!(session.shareWizardOpened || session.handoffStarted || session.viewerUrlCopied || session.shareCardExported)) {
    return summary;
  }

  const bool blocked = !health.serverProcessRunning || !health.localHealthReady || !health.hostIpReachable;
  if (!blocked) {
    summary.state = L"ready-for-handoff";
    summary.label = L"Ready For Handoff";
    summary.detail = L"The session looks healthy. Copy the Viewer URL, show the QR, or keep Share Wizard open while the other device connects.";
    return summary;
  }

  summary.state = L"needs-fix";
  summary.label = L"Needs Fix";
  if (!health.serverProcessRunning) {
    summary.detail = L"The share handoff has started, but the local service is not running. Start sharing again before sending the link.";
  } else if (!health.hostIpReachable) {
    summary.detail = L"The share handoff has started, but the selected LAN address is still not reachable. Refresh network selection before handing off again.";
  } else {
    summary.detail = L"The share handoff has started, but one or more live checks are still failing. Re-run checks before handing off again.";
  }
  return summary;
}

std::wstring ComputeDashboardOverallState(const RuntimeSessionState& session,
                                          const RuntimeHealthState& health,
                                          int p0FailureCount) {
  if (IsHostStateSharing(session.hostPageState)) {
    return L"Sharing";
  }
  if (p0FailureCount == 0 && health.serverProcessRunning && IsHostStateReadyOrLoading(session.hostPageState)) {
    return L"Ready";
  }
  if (health.serverProcessRunning && (!health.localHealthReady || !health.hostIpReachable)) {
    return L"Error";
  }
  return L"Not Ready";
}

std::wstring BuildShareInfoText(const RuntimeSessionState& session,
                                const RuntimeHealthState& health,
                                const RuntimeSelfCheckSummary& selfCheck) {
  std::wstringstream ss;
  ss << L"Mode: " << (session.networkMode.empty() ? L"unknown" : session.networkMode) << L"\r\n";
  ss << L"Host IPv4: " << (session.hostIp.empty() ? L"(not found)" : session.hostIp) << L"\r\n";
  ss << L"Port: " << session.port << L"\r\n";
  ss << L"Room: " << session.room << L"\r\n";
  ss << L"Token: " << session.token << L"\r\n";
  ss << L"Host Page: " << session.hostPageState << L"\r\n";
  ss << L"Hotspot: " << session.hotspotStatus << L"\r\n";
  ss << L"SSID: " << (session.hotspotSsid.empty() ? L"(not configured)" : session.hotspotSsid) << L"\r\n";
  ss << L"Password: " << (session.hotspotPassword.empty() ? L"(not configured)" : session.hotspotPassword) << L"\r\n";
  ss << L"Wi-Fi Direct API: " << (session.wifiDirectApiAvailable ? L"available" : L"not detected") << L"\r\n";
  ss << L"Wi-Fi Direct Alias: " << session.wifiDirectAlias << L"\r\n";
  ss << L"QR Renderer: local SVG (offline)\r\n";
  ss << L"Port Check: " << (health.portReady ? L"ready" : L"attention") << L" (" << health.portDetail << L")\r\n";
  ss << L"Local Health: " << (health.localHealthReady ? L"ok" : L"attention") << L" (" << health.localHealthDetail << L")\r\n";
  ss << L"LAN Bind: " << (health.lanBindReady ? L"ready" : L"attention") << L" (" << health.lanBindDetail << L")\r\n";
  ss << L"LAN Endpoint: " << (health.hostIpReachable ? L"ok" : L"attention") << L" (" << health.hostIpReachableDetail << L")\r\n";
  ss << L"Adapter Hint: " << health.adapterHint << L"\r\n";
  ss << L"Embedded Host: " << (health.embeddedHostReady ? L"ready" : L"attention") << L" (" << health.embeddedHostStatus << L")\r\n";
  ss << L"Self-Check: " << selfCheck.summaryLine << L"\r\n";
  ss << L"Check Categories: net " << selfCheck.networkCount
     << L" / sharing " << selfCheck.sharingCount << L"\r\n";
  ss << L"Live Pages: auto refresh from share_status.js\r\n";
  ss << L"Bundle Files: share_card.html / share_wizard.html / desktop_self_check.html / share_bundle.json / share_status.js / share_diagnostics.txt / desktop_self_check.txt\r\n";
  ss << L"Rooms / Viewers: " << session.lastRooms << L" / " << session.lastViewers << L"\r\n\r\n";
  ss << L"Host URL\r\n" << BuildHostUrl(session) << L"\r\n\r\n";
  ss << L"Viewer URL\r\n" << BuildViewerUrl(session);
  return ss.str();
}

} // namespace lan::runtime
