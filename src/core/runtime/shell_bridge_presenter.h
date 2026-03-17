#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace lan::runtime {

struct ShellBridgeAdapterCandidate {
  std::wstring name;
  std::wstring ip;
  std::wstring type;
  bool recommended = false;
  bool selected = false;
  bool probeReady = false;
  std::wstring probeLabel;
  std::wstring probeDetail;
};

struct ShellBridgeSnapshotState {
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
  bool certReady = false;
  std::wstring certDetail;
  std::wstring certExpectedSans;
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
  std::wstring certDir;
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
  std::vector<ShellBridgeAdapterCandidate> networkCandidates;

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
  std::wstring certBypassPolicy;
  std::wstring webViewBehavior;
  std::wstring startupHook;
};

enum class ShellBridgeSource {
  Unknown,
  AdminShell,
  HostPage,
};

enum class ShellBridgeInboundKind {
  None,
  AdminReady,
  AdminRequestSnapshot,
  AdminCommand,
  HostStatus,
  HostLog,
  Unknown,
};

enum class ShellBridgeAdminCommandKind {
  None,
  RefreshNetwork,
  GenerateRoomToken,
  ApplySession,
  StartServer,
  StopServer,
  ServiceOnly,
  StartAndOpenHost,
  OpenHost,
  OpenViewer,
  CopyHostUrl,
  CopyViewerUrl,
  ExportBundle,
  OpenOutput,
  OpenReport,
  RefreshBundle,
  ShowShareWizard,
  ShowQr,
  QuickFixNetwork,
  QuickFixCertificate,
  QuickFixSharing,
  QuickFixHandoff,
  QuickFixHotspot,
  SelectAdapter,
  ApplyHotspot,
  StartHotspot,
  StopHotspot,
  AutoHotspot,
  OpenHotspotSettings,
  OpenFirewallSettings,
  RunNetworkDiagnostics,
  CheckWebViewRuntime,
  TrustLocalCertificate,
  ExportRemoteProbeGuide,
  OpenConnectedDevices,
  SwitchPage,
};

struct ShellBridgeAdminCommand {
  ShellBridgeAdminCommandKind kind = ShellBridgeAdminCommandKind::None;
  std::wstring commandName;
  std::wstring room;
  std::wstring token;
  std::wstring bind;
  int port = 9443;
  std::size_t index = 0;
  bool hasIndex = false;
  std::wstring ssid;
  std::wstring password;
  std::wstring page;
};

struct ShellBridgeInboundMessage {
  ShellBridgeSource source = ShellBridgeSource::Unknown;
  ShellBridgeInboundKind kind = ShellBridgeInboundKind::None;
  ShellBridgeAdminCommand adminCommand;
  std::wstring hostState;
  std::wstring captureState;
  std::wstring captureLabel;
  bool hasCaptureState = false;
  bool hasCaptureLabel = false;
  std::size_t viewers = 0;
  bool hasViewers = false;
  std::wstring logMessage;
  std::wstring rawPayload;
};

ShellBridgeInboundMessage ParseShellBridgeInboundMessage(std::wstring_view payload);
std::wstring BuildShellBridgeSnapshotEventJson(const ShellBridgeSnapshotState& snapshot);

} // namespace lan::runtime
