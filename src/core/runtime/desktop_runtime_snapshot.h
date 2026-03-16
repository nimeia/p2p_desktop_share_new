#pragma once

#include "core/runtime/share_artifact_service.h"

#include <string>

namespace lan::runtime {

struct DesktopRuntimeSnapshotInput {
  std::wstring networkMode;
  std::wstring hostIp;
  std::wstring bindAddress;
  int port = 0;
  std::wstring room;
  std::wstring token;
  std::wstring hostPageState;
  std::wstring hotspotStatus;
  std::wstring hotspotSsid;
  std::wstring hotspotPassword;
  std::wstring wifiDirectAlias;
  std::wstring webviewStatusText;
  bool wifiDirectApiAvailable = false;
  bool wifiAdapterPresent = false;
  bool hotspotSupported = false;
  bool hotspotRunning = false;
  bool viewerUrlCopied = false;
  bool shareCardExported = false;
  bool shareWizardOpened = false;
  bool handoffStarted = false;
  bool handoffDelivered = false;
  std::size_t lastRooms = 0;
  std::size_t lastViewers = 0;

  bool serverProcessRunning = false;
  bool certReady = false;
  std::wstring certDetail;
  std::wstring expectedSans;
  bool portReady = false;
  std::wstring portDetail;
  bool localHealthReady = false;
  std::wstring localHealthDetail;
  bool hostIpReachable = false;
  std::wstring hostIpReachableDetail;
  bool lanBindReady = false;
  std::wstring lanBindDetail;
  std::size_t activeIpv4Candidates = 0;
  bool selectedIpRecommended = false;
  std::wstring adapterHint;
  bool embeddedHostReady = false;
  std::wstring embeddedHostStatus;
  bool firewallReady = false;
  std::wstring firewallDetail;
  bool liveReady = true;
};

struct DesktopRuntimeSnapshot {
  RuntimeSessionState session;
  RuntimeHealthState health;
  SelfCheckReport selfCheckReport;
  RuntimeSelfCheckSummary selfCheckSummary;
  RuntimeHandoffSummary handoff;
  std::wstring hostUrl;
  std::wstring viewerUrl;
  std::wstring dashboardOverall;
  std::wstring recentHeartbeat;
  std::wstring localReachability;
};

DesktopRuntimeSnapshot BuildDesktopRuntimeSnapshot(const DesktopRuntimeSnapshotInput& input);

} // namespace lan::runtime
