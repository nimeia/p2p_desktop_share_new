#include "core/runtime/desktop_runtime_snapshot.h"

namespace lan::runtime {
namespace {

RuntimeSessionState BuildSessionState(const DesktopRuntimeSnapshotInput& input) {
  RuntimeSessionState session;
  session.localeCode = input.localeCode;
  session.networkMode = input.networkMode;
  session.hostIp = input.hostIp;
  session.bindAddress = input.bindAddress;
  session.port = input.port;
  session.room = input.room;
  session.token = input.token;
  session.hostPageState = input.hostPageState;
  session.captureState = input.captureState;
  session.captureLabel = input.captureLabel;
  session.hotspotStatus = input.hotspotStatus;
  session.hotspotSsid = input.hotspotSsid;
  session.hotspotPassword = input.hotspotPassword;
  session.wifiDirectAlias = input.wifiDirectAlias;
  session.webviewStatusText = input.webviewStatusText;
  session.wifiDirectApiAvailable = input.wifiDirectApiAvailable;
  session.wifiAdapterPresent = input.wifiAdapterPresent;
  session.hotspotSupported = input.hotspotSupported;
  session.hotspotRunning = input.hotspotRunning;
  session.viewerUrlCopied = input.viewerUrlCopied;
  session.shareCardExported = input.shareCardExported;
  session.shareWizardOpened = input.shareWizardOpened;
  session.handoffStarted = input.handoffStarted;
  session.handoffDelivered = input.handoffDelivered;
  session.lastRooms = input.lastRooms;
  session.lastViewers = input.lastViewers;
  return session;
}

RuntimeHealthState BuildHealthState(const DesktopRuntimeSnapshotInput& input) {
  RuntimeHealthState health;
  health.serverProcessRunning = input.serverProcessRunning;
  health.certReady = input.certReady;
  health.certDetail = input.certDetail;
  health.expectedSans = input.expectedSans;
  health.portReady = input.portReady;
  health.portDetail = input.portDetail;
  health.localHealthReady = input.localHealthReady;
  health.localHealthDetail = input.localHealthDetail;
  health.hostIpReachable = input.hostIpReachable;
  health.hostIpReachableDetail = input.hostIpReachableDetail;
  health.lanBindReady = input.lanBindReady;
  health.lanBindDetail = input.lanBindDetail;
  health.activeIpv4Candidates = input.activeIpv4Candidates;
  health.selectedIpRecommended = input.selectedIpRecommended;
  health.adapterHint = input.adapterHint;
  health.embeddedHostReady = input.embeddedHostReady;
  health.embeddedHostStatus = input.embeddedHostStatus;
  health.firewallReady = input.firewallReady;
  health.firewallDetail = input.firewallDetail;
  return health;
}

RuntimeSelfCheckSummary BuildSummary(const SelfCheckReport& report) {
  RuntimeSelfCheckSummary summary;
  summary.p0 = report.p0;
  summary.certificateCount = report.certificateCount;
  summary.networkCount = report.networkCount;
  summary.sharingCount = report.sharingCount;
  summary.summaryLine = BuildSelfCheckSummaryLine(report);
  return summary;
}

} // namespace

DesktopRuntimeSnapshot BuildDesktopRuntimeSnapshot(const DesktopRuntimeSnapshotInput& input) {
  DesktopRuntimeSnapshot snapshot;
  snapshot.session = BuildSessionState(input);
  snapshot.health = BuildHealthState(input);
  snapshot.hostUrl = BuildHostUrl(snapshot.session);
  snapshot.viewerUrl = BuildViewerUrl(snapshot.session);
  snapshot.selfCheckReport = BuildSelfCheckReport(snapshot.session, snapshot.health, input.liveReady);
  snapshot.selfCheckSummary = BuildSummary(snapshot.selfCheckReport);
  snapshot.handoff = BuildHandoffSummary(snapshot.session, snapshot.health);
  snapshot.dashboardOverall = ComputeDashboardOverallState(snapshot.session, snapshot.health, snapshot.selfCheckReport.p0);
  snapshot.recentHeartbeat = snapshot.health.localHealthReady ? L"/health ok" : snapshot.health.localHealthDetail;
  snapshot.localReachability = snapshot.health.hostIpReachable ? L"ok" : snapshot.health.hostIpReachableDetail;
  return snapshot;
}

} // namespace lan::runtime
