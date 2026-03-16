#include "core/runtime/admin_shell_runtime_publisher.h"

namespace lan::runtime {

AdminShellRuntimeRefreshPolicy ResolveAdminShellRuntimeRefreshPolicy(bool requestSnapshot, bool stateChanged) {
  AdminShellRuntimeRefreshPolicy policy;
  policy.markShellReady = requestSnapshot;
  policy.shouldPublish = requestSnapshot || stateChanged;
  return policy;
}

ShellBridgeSnapshotState BuildAdminShellSnapshotState(const AdminViewModelInput& input) {
  const auto viewModel = BuildAdminSnapshotViewModel(input);

  ShellBridgeSnapshotState snapshot;
  snapshot.appName = viewModel.appName;
  snapshot.nativePage = viewModel.nativePage;
  snapshot.dashboardState = viewModel.dashboardState;
  snapshot.dashboardLabel = viewModel.dashboardLabel;
  snapshot.dashboardError = viewModel.dashboardError;
  snapshot.canStartSharing = viewModel.canStartSharing;
  snapshot.sharingActive = viewModel.sharingActive;
  snapshot.serverRunning = viewModel.serverRunning;
  snapshot.healthReady = viewModel.healthReady;
  snapshot.hostReachable = viewModel.hostReachable;
  snapshot.certReady = viewModel.certReady;
  snapshot.certDetail = viewModel.certDetail;
  snapshot.certExpectedSans = viewModel.certExpectedSans;
  snapshot.firewallReady = viewModel.firewallReady;
  snapshot.firewallDetail = viewModel.firewallDetail;
  snapshot.remoteViewerReady = viewModel.remoteViewerReady;
  snapshot.remoteViewerDetail = viewModel.remoteViewerDetail;
  snapshot.remoteProbeLabel = viewModel.remoteProbeLabel;
  snapshot.remoteProbeAction = viewModel.remoteProbeAction;
  snapshot.wifiAdapterPresent = viewModel.wifiAdapterPresent;
  snapshot.hotspotSupported = viewModel.hotspotSupported;
  snapshot.hotspotRunning = viewModel.hotspotRunning;
  snapshot.wifiDirectAvailable = viewModel.wifiDirectAvailable;
  snapshot.activeIpv4Candidates = viewModel.activeIpv4Candidates;
  snapshot.port = viewModel.port;
  snapshot.rooms = viewModel.rooms;
  snapshot.viewers = viewModel.viewers;
  snapshot.hostIp = viewModel.hostIp;
  snapshot.bind = viewModel.bind;
  snapshot.room = viewModel.room;
  snapshot.token = viewModel.token;
  snapshot.hostUrl = viewModel.hostUrl;
  snapshot.viewerUrl = viewModel.viewerUrl;
  snapshot.networkMode = viewModel.networkMode;
  snapshot.hostState = viewModel.hostState;
  snapshot.hotspotStatus = viewModel.hotspotStatus;
  snapshot.hotspotSsid = viewModel.hotspotSsid;
  snapshot.hotspotPassword = viewModel.hotspotPassword;
  snapshot.webviewStatus = viewModel.webviewStatus;
  snapshot.recentHeartbeat = viewModel.recentHeartbeat;
  snapshot.localReachability = viewModel.localReachability;
  snapshot.outputDir = viewModel.outputDir;
  snapshot.bundleDir = viewModel.bundleDir;
  snapshot.serverExePath = viewModel.serverExePath;
  snapshot.certDir = viewModel.certDir;
  snapshot.timelineText = viewModel.timelineText;
  snapshot.logTail = viewModel.logTail;
  snapshot.viewerUrlCopied = viewModel.viewerUrlCopied;
  snapshot.shareBundleExported = viewModel.shareBundleExported;
  snapshot.shareWizardOpened = viewModel.shareWizardOpened;
  snapshot.handoffStarted = viewModel.handoffStarted;
  snapshot.handoffDelivered = viewModel.handoffDelivered;
  snapshot.handoffState = viewModel.handoffState;
  snapshot.handoffLabel = viewModel.handoffLabel;
  snapshot.handoffDetail = viewModel.handoffDetail;
  snapshot.lastError = viewModel.lastError;
  snapshot.defaultPort = viewModel.defaultPort;
  snapshot.defaultBind = viewModel.defaultBind;
  snapshot.roomRule = viewModel.roomRule;
  snapshot.tokenRule = viewModel.tokenRule;
  snapshot.logLevel = viewModel.logLevel;
  snapshot.defaultViewerOpenMode = viewModel.defaultViewerOpenMode;
  snapshot.autoCopyViewerLink = viewModel.autoCopyViewerLink;
  snapshot.autoGenerateQr = viewModel.autoGenerateQr;
  snapshot.autoExportBundle = viewModel.autoExportBundle;
  snapshot.saveStdStreams = viewModel.saveStdStreams;
  snapshot.certBypassPolicy = viewModel.certBypassPolicy;
  snapshot.webViewBehavior = viewModel.webViewBehavior;
  snapshot.startupHook = viewModel.startupHook;

  snapshot.networkCandidates.reserve(viewModel.networkCandidates.size());
  for (const auto& candidate : viewModel.networkCandidates) {
    ShellBridgeAdapterCandidate item;
    item.name = candidate.name;
    item.ip = candidate.ip;
    item.type = candidate.type;
    item.recommended = candidate.recommended;
    item.selected = candidate.selected;
    item.probeReady = candidate.probeReady;
    item.probeLabel = candidate.probeLabel;
    item.probeDetail = candidate.probeDetail;
    snapshot.networkCandidates.push_back(item);
  }

  return snapshot;
}

AdminShellRuntimePublishResult PublishAdminShellRuntime(const AdminShellRuntimePublishContext& context,
                                                        const AdminShellRuntimePublisherHooks& hooks) {
  AdminShellRuntimePublishResult result;
  if (!context.adminShellActive || !context.adminShellReady) {
    return result;
  }

  result.snapshot = BuildAdminShellSnapshotState(context.viewModelInput);
  result.eventJson = BuildShellBridgeSnapshotEventJson(result.snapshot);
  result.published = !result.eventJson.empty();
  if (result.published && hooks.publishJson) {
    hooks.publishJson(result.eventJson);
  }
  return result;
}

} // namespace lan::runtime
