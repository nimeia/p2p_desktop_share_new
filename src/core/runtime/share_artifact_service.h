#pragma once

#include "core/runtime/runtime_controller.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace lan::runtime {

struct SelfCheckItem {
  std::string id;
  std::string title;
  std::string status;
  std::string detail;
  std::string severity;
  std::string category;
  bool ok = false;
};

struct FailureHint {
  std::string title;
  std::string detail;
  std::string action;
  std::string severity;
  std::string category;
};

struct SelfCheckReport {
  std::vector<SelfCheckItem> items;
  std::vector<FailureHint> failures;
  int passed = 0;
  int total = 0;
  int p0 = 0;
  int p1 = 0;
  int p2 = 0;
  int networkCount = 0;
  int sharingCount = 0;
};

struct ShareArtifactWriteRequest {
  std::filesystem::path outputDir;
  std::filesystem::path qrAssetSource;
  RuntimeSessionState session;
  RuntimeHealthState health;
  std::wstring generatedAt;
  bool liveReady = true;
};

struct ShareArtifactWriteResult {
  std::filesystem::path shareCardPath;
  std::filesystem::path shareWizardPath;
  std::filesystem::path bundleJsonPath;
  std::filesystem::path desktopSelfCheckPath;
  std::wstring errorMessage;
};

SelfCheckReport BuildSelfCheckReport(std::wstring_view hostState,
                                     std::wstring_view hostIp,
                                     std::wstring_view viewerUrl,
                                     std::size_t viewers,
                                     bool serverProcessRunning,
                                     bool portReady,
                                     std::wstring_view portDetail,
                                     bool localHealthReady,
                                     std::wstring_view localHealthDetail,
                                     bool hostIpReachable,
                                     std::wstring_view hostIpReachableDetail,
                                     bool lanBindReady,
                                     std::wstring_view lanBindDetail,
                                     int activeIpv4Candidates,
                                     bool selectedIpRecommended,
                                     std::wstring_view adapterHint,
                                     bool embeddedHostReady,
                                     std::wstring_view embeddedHostStatus,
                                     bool firewallReady,
                                     std::wstring_view firewallDetail,
                                     bool wifiAdapterPresent,
                                     bool hotspotSupported,
                                     bool wifiDirectApiAvailable,
                                     bool hotspotRunning,
                                     bool liveReady);

SelfCheckReport BuildSelfCheckReport(const RuntimeSessionState& session,
                                     const RuntimeHealthState& health,
                                     bool liveReady);

std::wstring BuildSelfCheckSummaryLine(const SelfCheckReport& report);

bool ExportShareArtifacts(const ShareArtifactWriteRequest& request,
                          ShareArtifactWriteResult* result);

} // namespace lan::runtime
