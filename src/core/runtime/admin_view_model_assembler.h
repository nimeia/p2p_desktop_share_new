#pragma once

#include "core/runtime/desktop_runtime_snapshot.h"
#include "core/runtime/host_session_coordinator.h"
#include "core/runtime/remote_probe_orchestrator.h"

#include <string>
#include <vector>

namespace lan::runtime {

enum class AdminDashboardSuggestionKind {
  None,
  StartServer,
  OpenQuickWizard,
  OpenDiagnostics,
  OpenHostExternally,
  StartHotspot,
  OpenHotspotSettings,
  RefreshIp,
  PortReady,
};

struct AdminViewNetworkCandidate {
  std::wstring name;
  std::wstring ip;
  std::wstring type;
  bool recommended = false;
  bool selected = false;
  bool probeReady = false;
  std::wstring probeLabel;
  std::wstring probeDetail;
};

struct AdminViewModelInput {
  std::wstring localeCode;
  std::wstring appName;
  std::wstring nativePage;
  DesktopRuntimeSnapshot runtimeSnapshot;
  HostSessionAdminModel sessionModel;
  std::wstring lastErrorSummary;
  std::wstring outputDir;
  std::wstring bundleDir;
  std::wstring serverExePath;
  std::wstring adminDir;
  std::wstring timelineText;
  std::wstring logTail;
  std::vector<AdminViewNetworkCandidate> networkCandidates;

  std::wstring defaultServerExePath;
  std::wstring defaultWwwPath;
  std::wstring defaultAdminDir;
  std::wstring defaultLaunchArgs;
  std::wstring defaultIpStrategy;
  int autoDetectFrequencySec = 15;
  std::wstring hotspotPasswordRule;
  std::wstring logLevel;
  std::wstring configuredDefaultViewerOpenMode;
  std::wstring outputDirSetting;
  int diagnosticsRetentionDays = 7;
  std::wstring snapshotWebViewBehavior;
  std::wstring configuredWebViewBehavior;
  std::wstring snapshotStartupHook;
  std::wstring configuredStartupHook;

  bool autoCopyViewerLink = false;
  bool autoGenerateQr = false;
  bool autoExportBundle = false;
  bool saveStdStreams = false;

  bool serverExeExists = false;
  bool wwwDirExists = false;
  bool adminDirExists = false;
  bool bundleDirExists = false;
};

struct AdminSnapshotViewModel {
  std::wstring localeCode;
  std::wstring appName;
  std::wstring nativePage;
  std::wstring dashboardState;
  std::wstring dashboardLabel;
  std::wstring dashboardError;
  bool canStartSharing = false;
  bool sharingActive = false;
  bool serverRunning = false;
  bool healthReady = false;
  bool hostReachable = false;
  bool firewallReady = false;
  std::wstring firewallDetail;
  bool remoteViewerReady = false;
  std::wstring remoteViewerDetail;
  std::wstring remoteProbeLabel;
  std::wstring remoteProbeAction;
  bool wifiAdapterPresent = false;
  bool hotspotSupported = false;
  bool hotspotRunning = false;
  bool wifiDirectAvailable = false;
  int activeIpv4Candidates = 0;
  int port = 0;
  std::size_t rooms = 0;
  std::size_t viewers = 0;
  std::wstring hostIp;
  std::wstring bind;
  std::wstring room;
  std::wstring token;
  std::wstring hostUrl;
  std::wstring viewerUrl;
  std::wstring networkMode;
  std::wstring hostState;
  std::wstring captureState;
  std::wstring captureLabel;
  std::wstring hotspotStatus;
  std::wstring hotspotSsid;
  std::wstring hotspotPassword;
  std::wstring webviewStatus;
  std::wstring recentHeartbeat;
  std::wstring localReachability;
  std::wstring outputDir;
  std::wstring bundleDir;
  std::wstring serverExePath;
  std::wstring adminDir;
  std::wstring timelineText;
  std::wstring logTail;
  bool viewerUrlCopied = false;
  bool shareBundleExported = false;
  bool shareWizardOpened = false;
  bool handoffStarted = false;
  bool handoffDelivered = false;
  std::wstring handoffState;
  std::wstring handoffLabel;
  std::wstring handoffDetail;
  std::wstring lastError;
  std::vector<AdminViewNetworkCandidate> networkCandidates;

  int defaultPort = 9443;
  std::wstring defaultBind;
  std::wstring roomRule;
  std::wstring tokenRule;
  std::wstring logLevel;
  std::wstring defaultViewerOpenMode;
  bool autoCopyViewerLink = false;
  bool autoGenerateQr = false;
  bool autoExportBundle = false;
  bool saveStdStreams = false;
  std::wstring webViewBehavior;
  std::wstring startupHook;
};

struct AdminDashboardSuggestionViewModel {
  AdminDashboardSuggestionKind kind = AdminDashboardSuggestionKind::None;
  std::wstring title;
  std::wstring detail;
  bool fixEnabled = false;
  bool infoEnabled = true;
  bool setupEnabled = true;
};

struct DashboardViewModel {
  std::wstring statusCard;
  std::wstring networkCard;
  std::wstring serviceCard;
  std::wstring shareCard;
  std::wstring healthCard;
  bool primaryActionEnabled = true;
  std::vector<AdminDashboardSuggestionViewModel> suggestions;
};

struct SettingsViewModel {
  std::wstring intro;
  std::wstring generalCard;
  std::wstring serviceCard;
  std::wstring networkCard;
  std::wstring sharingCard;
  std::wstring loggingCard;
  std::wstring advancedCard;
  std::wstring currentStateCard;
};

AdminSnapshotViewModel BuildAdminSnapshotViewModel(const AdminViewModelInput& input);
DashboardViewModel BuildDashboardViewModel(const AdminViewModelInput& input);
SettingsViewModel BuildSettingsViewModel(const AdminViewModelInput& input);

} // namespace lan::runtime
