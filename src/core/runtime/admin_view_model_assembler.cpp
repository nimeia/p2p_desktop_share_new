#include "core/runtime/admin_view_model_assembler.h"

#include "core/runtime/network_diagnostics_policy.h"

#include <algorithm>
#include <sstream>

namespace lan::runtime {
namespace {

bool IsSharingState(std::wstring_view state) {
  return state == L"sharing" || state == L"shared" || state == L"streaming";
}

bool IsCaptureActive(std::wstring_view state) {
  return state == L"active" || state == L"sharing";
}

std::wstring DisplayOrFallback(std::wstring_view value, std::wstring_view fallback) {
  return value.empty() ? std::wstring(fallback) : std::wstring(value);
}

std::wstring DashboardStateFromOverall(std::wstring_view overall) {
  if (overall == L"Sharing") return L"sharing";
  if (overall == L"Ready") return L"ready";
  if (overall == L"Error") return L"error";
  return L"not-ready";
}

std::wstring BuildDashboardError(const AdminViewModelInput& input) {
  if (!input.runtimeSnapshot.selfCheckReport.failures.empty()) {
    return std::wstring(input.runtimeSnapshot.selfCheckReport.failures.front().title.begin(),
                        input.runtimeSnapshot.selfCheckReport.failures.front().title.end());
  }
  return input.lastErrorSummary.empty() ? L"none" : input.lastErrorSummary;
}

AdminDashboardSuggestionViewModel MakeSuggestion(AdminDashboardSuggestionKind kind,
                                                 std::wstring title,
                                                 std::wstring detail,
                                                 bool fixEnabled = true,
                                                 bool infoEnabled = true,
                                                 bool setupEnabled = true) {
  AdminDashboardSuggestionViewModel suggestion;
  suggestion.kind = kind;
  suggestion.title = std::move(title);
  suggestion.detail = std::move(detail);
  suggestion.fixEnabled = fixEnabled;
  suggestion.infoEnabled = infoEnabled;
  suggestion.setupEnabled = setupEnabled;
  return suggestion;
}

} // namespace

AdminSnapshotViewModel BuildAdminSnapshotViewModel(const AdminViewModelInput& input) {
  const auto& runtime = input.runtimeSnapshot;
  const auto& session = runtime.session;
  const auto& health = runtime.health;
  const auto& sessionModel = input.sessionModel;
  const auto networkDiagnostics = BuildNetworkDiagnosticsViewModel(session, health);

  std::vector<RemoteProbeCandidateInput> remoteProbeInputs;
  remoteProbeInputs.reserve(input.networkCandidates.size());
  for (const auto& candidate : input.networkCandidates) {
    RemoteProbeCandidateInput probe;
    probe.name = candidate.name;
    probe.ip = candidate.ip;
    probe.type = candidate.type;
    probe.recommended = candidate.recommended;
    probe.selected = candidate.selected;
    probe.probeReady = candidate.probeReady;
    probe.probeDetail = candidate.probeDetail;
    remoteProbeInputs.push_back(std::move(probe));
  }
  const auto remoteProbePlan = BuildRemoteProbePlan(remoteProbeInputs);

  AdminSnapshotViewModel model;
  model.localeCode = input.localeCode;
  model.appName = input.appName;
  model.nativePage = input.nativePage;
  model.dashboardState = DashboardStateFromOverall(runtime.dashboardOverall);
  model.dashboardLabel = runtime.dashboardOverall;
  model.dashboardError = BuildDashboardError(input);

  model.handoffState = runtime.handoff.state;
  model.handoffLabel = runtime.handoff.label;
  model.handoffDetail = runtime.handoff.detail;
  model.shareWizardOpened = sessionModel.shareWizardOpened;
  model.handoffStarted = sessionModel.handoffStarted || sessionModel.viewerUrlCopied || sessionModel.shareBundleExported;
  model.handoffDelivered = sessionModel.handoffDelivered || session.lastViewers > 0;

  const bool sharingActive = IsSharingState(session.hostPageState) || IsCaptureActive(session.captureState);
  model.canStartSharing = !sharingActive;
  model.sharingActive = sharingActive;
  model.serverRunning = health.serverProcessRunning;
  model.healthReady = health.localHealthReady;
  model.hostReachable = health.hostIpReachable;
  model.certReady = health.certReady;
  model.certDetail = health.certDetail;
  model.certExpectedSans = health.expectedSans;
  model.firewallReady = networkDiagnostics.firewallReady;
  model.firewallDetail = networkDiagnostics.firewallDetail;
  model.remoteViewerReady = networkDiagnostics.remoteViewerReady;
  model.remoteViewerDetail = networkDiagnostics.remoteViewerDetail;
  model.remoteProbeLabel = remoteProbePlan.label;
  model.remoteProbeAction = remoteProbePlan.action;
  model.wifiAdapterPresent = session.wifiAdapterPresent;
  model.hotspotSupported = session.hotspotSupported;
  model.hotspotRunning = session.hotspotRunning;
  model.wifiDirectAvailable = session.wifiDirectApiAvailable;
  model.activeIpv4Candidates = static_cast<int>(health.activeIpv4Candidates);
  model.port = sessionModel.port;
  model.rooms = session.lastRooms;
  model.viewers = session.lastViewers;
  model.hostIp = DisplayOrFallback(session.hostIp, L"(not found)");
  model.bind = sessionModel.bindAddress;
  model.room = sessionModel.room;
  model.token = sessionModel.token;
  model.hostUrl = runtime.hostUrl;
  model.viewerUrl = runtime.viewerUrl;
  model.networkMode = DisplayOrFallback(session.networkMode, L"unknown");
  model.hostState = session.hostPageState;
  model.captureState = session.captureState;
  model.captureLabel = session.captureLabel;
  model.hotspotStatus = session.hotspotStatus;
  model.hotspotSsid = session.hotspotSsid;
  model.hotspotPassword = session.hotspotPassword;
  model.webviewStatus = session.webviewStatusText;
  model.recentHeartbeat = runtime.recentHeartbeat;
  model.localReachability = runtime.localReachability;
  model.outputDir = input.outputDir;
  model.bundleDir = input.bundleDir;
  model.serverExePath = input.serverExePath;
  model.certDir = input.certDir;
  model.timelineText = input.timelineText.empty() ? L"No timeline events yet." : input.timelineText;
  model.logTail = input.logTail;
  model.viewerUrlCopied = sessionModel.viewerUrlCopied;
  model.shareBundleExported = sessionModel.shareBundleExported;
  model.lastError = input.lastErrorSummary;
  model.networkCandidates = input.networkCandidates;

  model.defaultPort = sessionModel.defaultPort;
  model.defaultBind = sessionModel.defaultBindAddress;
  model.roomRule = sessionModel.roomRule;
  model.tokenRule = sessionModel.tokenRule;
  model.logLevel = input.logLevel;
  model.defaultViewerOpenMode = L"browser";
  model.autoCopyViewerLink = input.autoCopyViewerLink;
  model.autoGenerateQr = input.autoGenerateQr;
  model.autoExportBundle = input.autoExportBundle;
  model.saveStdStreams = input.saveStdStreams;
  model.certBypassPolicy = input.certBypassPolicy;
  model.webViewBehavior = input.snapshotWebViewBehavior;
  model.startupHook = input.snapshotStartupHook;
  return model;
}

DashboardViewModel BuildDashboardViewModel(const AdminViewModelInput& input) {
  const auto& runtime = input.runtimeSnapshot;
  const auto& session = runtime.session;
  const auto& health = runtime.health;
  const auto networkDiagnostics = BuildNetworkDiagnosticsViewModel(session, health);

  DashboardViewModel model;
  const std::wstring errorSummary = BuildDashboardError(input);

  std::wstringstream statusCard;
  statusCard << L"Current State: " << runtime.dashboardOverall << L"\r\n";
  statusCard << L"Host IP: " << DisplayOrFallback(session.hostIp, L"(not found)") << L"\r\n";
  statusCard << L"Port: " << session.port << L"\r\n";
  statusCard << L"Room: " << session.room << L"\r\n";
  statusCard << L"Viewer Count: " << session.lastViewers << L"\r\n";
  statusCard << L"Latest Error: " << errorSummary;
  model.statusCard = statusCard.str();

  std::wstringstream networkCard;
  networkCard << L"Network\r\n";
  networkCard << L"Primary IPv4: " << DisplayOrFallback(session.hostIp, L"(not found)") << L"\r\n";
  networkCard << L"Adapter Count: " << health.activeIpv4Candidates << L"\r\n";
  networkCard << L"Firewall: " << (networkDiagnostics.firewallReady ? L"ready" : L"attention") << L"\r\n";
  networkCard << L"Remote Viewer Path: " << (networkDiagnostics.remoteViewerReady ? L"ready" : L"attention") << L"\r\n";
  networkCard << L"Wi-Fi / Hotspot: "
              << (session.wifiAdapterPresent ? L"Wi-Fi yes" : L"Wi-Fi no")
              << L", hotspot " << (session.hotspotSupported ? L"yes" : L"fallback")
              << L", Wi-Fi Direct " << (session.wifiDirectApiAvailable ? L"yes" : L"no");
  model.networkCard = networkCard.str();

  std::wstringstream serviceCard;
  serviceCard << L"Service\r\n";
  serviceCard << L"Server Exe: " << input.serverExePath << L"\r\n";
  serviceCard << L"Bind + Port: " << session.bindAddress << L":" << session.port << L"\r\n";
  serviceCard << L"Transport: plain HTTP / WS";
  if (!health.certDetail.empty()) {
    serviceCard << L"\r\nMode Detail: " << health.certDetail;
  }
  model.serviceCard = serviceCard.str();

  std::wstringstream shareCard;
  shareCard << L"Sharing\r\n";
  shareCard << L"Viewer URL: " << runtime.viewerUrl << L"\r\n";
  shareCard << L"Capture State: " << DisplayOrFallback(session.captureState, L"not-started") << L"\r\n";
  shareCard << L"Capture Target: " << DisplayOrFallback(session.captureLabel, L"(not selected)") << L"\r\n";
  shareCard << L"Copied: " << (session.viewerUrlCopied ? L"yes" : L"no") << L"\r\n";
  shareCard << L"Share Card Exported: " << (session.shareCardExported ? L"yes" : L"no");
  model.shareCard = shareCard.str();

  std::wstringstream healthCard;
  healthCard << L"Health\r\n";
  healthCard << L"/health: " << (health.localHealthReady ? L"ok" : L"attention") << L"\r\n";
  healthCard << L"/api/status: " << (health.serverProcessRunning ? L"polling" : L"server stopped") << L"\r\n";
  healthCard << L"WebView: " << session.webviewStatusText << L"\r\n";
  healthCard << L"Local Reachability: " << (health.hostIpReachable ? L"ok" : L"attention") << L"\r\n";
  healthCard << L"Firewall: " << (networkDiagnostics.firewallReady ? L"ok" : L"attention") << L"\r\n";
  healthCard << L"Remote Viewer Path: " << (networkDiagnostics.remoteViewerReady ? L"ok" : L"attention");
  model.healthCard = healthCard.str();

  model.primaryActionEnabled = !(IsSharingState(session.hostPageState) || IsCaptureActive(session.captureState));

  if (!health.embeddedHostReady) {
    model.suggestions.push_back(MakeSuggestion(
        AdminDashboardSuggestionKind::OpenHostExternally,
        L"WebView2 runtime is unavailable",
        L"Embedded host view is unavailable. Open the host page in an external browser, or install/repair WebView2 Runtime."));
  }
  if (!networkDiagnostics.firewallReady) {
    model.suggestions.push_back(MakeSuggestion(
        AdminDashboardSuggestionKind::OpenDiagnostics,
        L"Inbound firewall path still needs attention",
        networkDiagnostics.firewallDetail.empty()
            ? L"No enabled inbound allow rule was detected for the server executable or the current TCP port."
            : networkDiagnostics.firewallDetail));
  }
  if (!session.hotspotRunning) {
    model.suggestions.push_back(MakeSuggestion(
        session.hotspotSupported ? AdminDashboardSuggestionKind::StartHotspot
                                 : AdminDashboardSuggestionKind::OpenHotspotSettings,
        L"Hotspot is not running",
        session.hotspotSupported
            ? L"You can try starting hotspot directly. If that fails, open Windows Mobile Hotspot settings."
            : L"This machine does not expose controllable hotspot support. Open system hotspot settings."));
  }
  model.suggestions.push_back(MakeSuggestion(
      AdminDashboardSuggestionKind::PortReady,
      L"Plain HTTP mode is active",
      L"The local admin shell and host page now run over local HTTP/WS. Focus on LAN reachability and firewall state instead."));
  if (health.portReady) {
    model.suggestions.push_back(MakeSuggestion(
        AdminDashboardSuggestionKind::PortReady,
        std::wstring(L"Port ") + std::to_wstring(session.port) + L" is available",
        health.portDetail.empty() ? L"The configured port is available for the local server." : health.portDetail));
  }
  if (model.suggestions.empty() || model.suggestions.size() < 4) {
    if (!health.serverProcessRunning) {
      model.suggestions.push_back(MakeSuggestion(
          AdminDashboardSuggestionKind::StartServer,
          L"Local server is not started",
          L"Use Start Sharing to launch the local HTTP/WS server and load the host page."));
    }
  }
  if (model.suggestions.size() < 4 && session.hostIp.empty()) {
    model.suggestions.push_back(MakeSuggestion(
        AdminDashboardSuggestionKind::RefreshIp,
        L"Host IP is still missing",
        L"Refresh the host IPv4, or connect to LAN / start hotspot before sharing."));
  }
  while (model.suggestions.size() < 4) {
    model.suggestions.push_back(MakeSuggestion(
        AdminDashboardSuggestionKind::OpenDiagnostics,
        L"No higher-priority suggestion right now.",
        L"Open diagnostics to inspect the full runtime snapshot.",
        false,
        true,
        true));
  }

  return model;
}

SettingsViewModel BuildSettingsViewModel(const AdminViewModelInput& input) {
  const auto& runtime = input.runtimeSnapshot;
  const auto& session = runtime.session;
  const auto& health = runtime.health;
  const auto& sessionModel = input.sessionModel;

  SettingsViewModel model;

  std::wstringstream intro;
  intro << L"Settings stays outside the session workflow. Use it to review default policies before operators start a room.\r\n";
  intro << L"Current page is a product-style settings center, not a persisted config backend yet.";
  model.intro = intro.str();

  std::wstringstream general;
  general << L"General\r\n\r\n";
  general << L"Default Port: " << sessionModel.defaultPort << L"\r\n";
  general << L"Default Bind: " << sessionModel.defaultBindAddress << L"\r\n";
  general << L"Room Rule: " << sessionModel.roomRule << L"\r\n";
  general << L"Token Rule: " << sessionModel.tokenRule << L"\r\n\r\n";
  general << L"Current Session\r\n";
  general << L"Room: " << sessionModel.room << L"\r\n";
  general << L"Token: " << sessionModel.token;
  model.generalCard = general.str();

  std::wstringstream service;
  service << L"Service\r\n\r\n";
  service << L"Server EXE\r\n" << input.defaultServerExePath << L"\r\n\r\n";
  service << L"WWW Dir\r\n" << input.defaultWwwPath << L"\r\n\r\n";
  service << L"Admin Dir\r\n" << input.defaultCertDir << L"\r\n\r\n";
  service << L"Args Template\r\n" << input.defaultLaunchArgs;
  model.serviceCard = service.str();

  std::wstringstream network;
  network << L"Network\r\n\r\n";
  network << L"Main IP Strategy: " << input.defaultIpStrategy << L"\r\n";
  network << L"Detect Frequency: " << input.autoDetectFrequencySec << L"s\r\n";
  network << L"Hotspot SSID: auto session alias\r\n";
  network << L"Password Rule: " << input.hotspotPasswordRule << L"\r\n\r\n";
  network << L"Live State\r\n";
  network << L"Selected IP: " << DisplayOrFallback(session.hostIp, L"(not selected)") << L"\r\n";
  network << L"Hotspot: " << session.hotspotStatus;
  model.networkCard = network.str();

  std::wstringstream sharing;
  sharing << L"Sharing\r\n\r\n";
  sharing << L"Viewer Open Mode: " << input.configuredDefaultViewerOpenMode << L"\r\n";
  sharing << L"Auto Copy Link: " << (input.autoCopyViewerLink ? L"enabled" : L"disabled") << L"\r\n";
  sharing << L"Auto QR: " << (input.autoGenerateQr ? L"enabled" : L"disabled") << L"\r\n";
  sharing << L"Auto Bundle Export: " << (input.autoExportBundle ? L"enabled" : L"disabled") << L"\r\n\r\n";
  sharing << L"Latest Viewer URL\r\n" << runtime.viewerUrl;
  model.sharingCard = sharing.str();

  std::wstringstream logging;
  logging << L"Logs & Diagnostics\r\n\r\n";
  logging << L"Log Level: " << input.logLevel << L"\r\n";
  logging << L"Output Dir\r\n" << input.outputDirSetting << L"\r\n\r\n";
  logging << L"Retention Days: " << input.diagnosticsRetentionDays << L"\r\n";
  logging << L"Save stdout/stderr: " << (input.saveStdStreams ? L"yes" : L"no") << L"\r\n\r\n";
  logging << L"Bundle Dir\r\n" << input.bundleDir;
  model.loggingCard = logging.str();

  std::wstringstream advanced;
  advanced << L"Advanced\r\n\r\n";
  advanced << L"Transport Policy: plain-http-first\r\n";
  advanced << L"WebView Mode: " << input.configuredWebViewBehavior << L"\r\n";
  advanced << L"Startup Hook: " << input.configuredStartupHook << L"\r\n\r\n";
  advanced << L"Runtime Flags\r\n";
  advanced << L"WebView Ready: " << (health.embeddedHostReady ? L"yes" : L"no") << L"\r\n";
  advanced << L"HTTP Mode: enabled";
  if (!health.certDetail.empty()) {
    advanced << L"\r\nMode Detail: " << health.certDetail;
  }
  model.advancedCard = advanced.str();

  std::wstringstream current;
  current << L"Current Effective Defaults Snapshot\r\n\r\n";
  current << L"Default Port -> current port: " << sessionModel.defaultPort << L" -> " << session.port << L"\r\n";
  current << L"Default Bind -> current bind: " << sessionModel.defaultBindAddress << L" -> " << session.bindAddress << L"\r\n";
  current << L"Server Path Exists: " << (input.serverExeExists ? L"yes" : L"no") << L"\r\n";
  current << L"WWW Path Exists: " << (input.wwwDirExists ? L"yes" : L"no") << L"\r\n";
  current << L"Admin Dir Exists: " << (input.certDirExists ? L"yes" : L"no") << L"\r\n";
  current << L"Bundle Dir Exists: " << (input.bundleDirExists ? L"yes" : L"no") << L"\r\n";
  current << L"Health Ready: " << (health.localHealthReady ? L"green / normal" : L"yellow / attention") << L"\r\n";
  current << L"Host Reachable: " << (health.hostIpReachable ? L"green / normal" : L"red / abnormal") << L"\r\n";
  current << L"Page Role: outside main flow, available for preflight and operator policy review.";
  model.currentStateCard = current.str();

  return model;
}

} // namespace lan::runtime
