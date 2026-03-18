#pragma once

#include <cstddef>
#include <string>

namespace lan::runtime {

struct RuntimeSessionState {
  std::wstring localeCode;
  std::wstring networkMode;
  std::wstring hostIp;
  std::wstring bindAddress;
  int port = 0;
  std::wstring room;
  std::wstring token;
  std::wstring hostPageState;
  std::wstring captureState;
  std::wstring captureLabel;
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
};

struct RuntimeHealthState {
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
};

struct RuntimeSelfCheckSummary {
  int p0 = 0;
  int certificateCount = 0;
  int networkCount = 0;
  int sharingCount = 0;
  std::wstring summaryLine;
};

struct RuntimeHandoffSummary {
  std::wstring state;
  std::wstring label;
  std::wstring detail;
};

std::wstring BuildHostUrl(const RuntimeSessionState& session);
std::wstring BuildViewerUrl(const RuntimeSessionState& session);
RuntimeHandoffSummary BuildHandoffSummary(const RuntimeSessionState& session,
                                          const RuntimeHealthState& health);
std::wstring ComputeDashboardOverallState(const RuntimeSessionState& session,
                                          const RuntimeHealthState& health,
                                          int p0FailureCount);
std::wstring BuildShareInfoText(const RuntimeSessionState& session,
                                const RuntimeHealthState& health,
                                const RuntimeSelfCheckSummary& selfCheck);

} // namespace lan::runtime
