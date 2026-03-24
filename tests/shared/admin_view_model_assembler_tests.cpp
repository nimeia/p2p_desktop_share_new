#include "core/runtime/admin_view_model_assembler.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "admin view model assembler test failed: " << message << "\n";
    std::exit(1);
  }
}

lan::runtime::AdminViewModelInput MakeInput() {
  using namespace lan::runtime;
  AdminViewModelInput input;
  input.appName = L"LanScreenShareHostApp";
  input.nativePage = L"dashboard";
  input.sessionModel.bindAddress = L"0.0.0.0";
  input.sessionModel.port = 9443;
  input.sessionModel.room = L"ROOM01";
  input.sessionModel.token = L"TOKEN01";
  input.sessionModel.defaultPort = 9443;
  input.sessionModel.defaultBindAddress = L"0.0.0.0";
  input.sessionModel.roomRule = L"random-6";
  input.sessionModel.tokenRule = L"random-16";
  input.sessionModel.viewerUrlCopied = true;
  input.sessionModel.shareBundleExported = true;
  input.runtimeSnapshot.session.hostIp = L"192.168.1.10";
  input.runtimeSnapshot.session.bindAddress = input.sessionModel.bindAddress;
  input.runtimeSnapshot.session.port = input.sessionModel.port;
  input.runtimeSnapshot.session.room = input.sessionModel.room;
  input.runtimeSnapshot.session.token = input.sessionModel.token;
  input.runtimeSnapshot.session.hostPageState = L"ready";
  input.runtimeSnapshot.session.hotspotStatus = L"running";
  input.runtimeSnapshot.session.hotspotSsid = L"TestHotspot";
  input.runtimeSnapshot.session.hotspotPassword = L"Password123";
  input.runtimeSnapshot.session.networkMode = L"lan";
  input.runtimeSnapshot.session.webviewStatusText = L"ready";
  input.runtimeSnapshot.session.wifiAdapterPresent = true;
  input.runtimeSnapshot.session.hotspotSupported = true;
  input.runtimeSnapshot.session.hotspotRunning = true;
  input.runtimeSnapshot.session.wifiDirectApiAvailable = true;
  input.runtimeSnapshot.session.viewerUrlCopied = true;
  input.runtimeSnapshot.session.shareCardExported = true;
  input.runtimeSnapshot.session.lastRooms = 1;
  input.runtimeSnapshot.session.lastViewers = 2;
  input.runtimeSnapshot.health.serverProcessRunning = true;
  input.runtimeSnapshot.health.portReady = true;
  input.runtimeSnapshot.health.portDetail = L"available";
  input.runtimeSnapshot.health.localHealthReady = true;
  input.runtimeSnapshot.health.hostIpReachable = true;
  input.runtimeSnapshot.health.activeIpv4Candidates = 2;
  input.runtimeSnapshot.health.embeddedHostReady = true;
  input.runtimeSnapshot.dashboardOverall = L"Ready";
  input.runtimeSnapshot.hostUrl = L"http://192.168.1.10:9443/host";
  input.runtimeSnapshot.viewerUrl = L"http://192.168.1.10:9443/view";
  input.runtimeSnapshot.recentHeartbeat = L"/health ok";
  input.runtimeSnapshot.localReachability = L"ok";
  input.runtimeSnapshot.handoff.state = L"delivered";
  input.runtimeSnapshot.handoff.label = L"Delivered";
  input.runtimeSnapshot.handoff.detail = L"Viewer connected";
  input.outputDir = L"C:/app/out";
  input.bundleDir = L"C:/app/out/share_bundle";
  input.serverExePath = L"C:/app/lan_screenshare_server.exe";
  input.adminDir = L"C:/app/webui";
  input.timelineText = L"Started";
  input.logTail = L"latest log";
  input.defaultServerExePath = input.serverExePath;
  input.defaultWwwPath = L"C:/app/www";
  input.defaultAdminDir = input.adminDir;
  input.defaultLaunchArgs = L"--bind {bind}";
  input.defaultIpStrategy = L"prefer-private-wifi";
  input.autoDetectFrequencySec = 15;
  input.hotspotPasswordRule = L"windows-suggested";
  input.logLevel = L"info";
  input.configuredDefaultViewerOpenMode = L"app-window-preferred";
  input.outputDirSetting = L"C:/app/out";
  input.diagnosticsRetentionDays = 7;
  input.snapshotWebViewBehavior = L"html-admin";
  input.configuredWebViewBehavior = L"embedded-when-available";
  input.snapshotStartupHook = L"none";
  input.configuredStartupHook = L"(not configured)";
  input.autoCopyViewerLink = true;
  input.autoGenerateQr = true;
  input.autoExportBundle = true;
  input.saveStdStreams = true;
  input.serverExeExists = true;
  input.wwwDirExists = true;
  input.adminDirExists = true;
  input.bundleDirExists = true;
  input.networkCandidates.push_back({L"Wi-Fi", L"192.168.1.10", L"wifi", true, true, true, L"LAN /health ok", L"Selected adapter answered"});
  return input;
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto input = MakeInput();
  const auto admin = BuildAdminSnapshotViewModel(input);
  Expect(admin.dashboardState == L"ready", "dashboard state should map from overall state");
  Expect(admin.dashboardLabel == L"Ready", "dashboard label should preserve overall label");
  Expect(admin.handoffDelivered, "handoff delivered should become true when viewers exist");
  Expect(admin.networkCandidates.size() == 1, "admin snapshot should expose adapter list");

  const auto dashboard = BuildDashboardViewModel(input);
  Expect(dashboard.primaryActionEnabled, "primary action should be enabled when not sharing");
  Expect(dashboard.suggestions.size() == 4, "dashboard should always provide four suggestions");
  Expect(dashboard.statusCard.find(L"Current State: Ready") != std::wstring::npos,
         "status card should include overall state");

  const auto settings = BuildSettingsViewModel(input);
  Expect(settings.generalCard.find(L"Default Port: 9443") != std::wstring::npos,
         "settings general card should include default port");
  Expect(settings.sharingCard.find(L"Viewer Open Mode: app-window-preferred") != std::wstring::npos,
         "settings sharing card should include configured viewer mode");
  Expect(settings.currentStateCard.find(L"Server Path Exists: yes") != std::wstring::npos,
         "settings current state should include existence flags");

  std::cout << "admin view model assembler tests passed\n";
  return 0;
}
