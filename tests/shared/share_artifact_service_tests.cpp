#include "core/runtime/share_artifact_service.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "share artifact service test failed: " << message << "\n";
    std::exit(1);
  }
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

} // namespace

int main() {
  namespace fs = std::filesystem;
  using namespace lan::runtime;

  RuntimeSessionState session;
  session.networkMode = L"lan";
  session.hostIp = L"192.168.1.20";
  session.port = 9443;
  session.room = L"roomA";
  session.token = L"tokenA";
  session.hostPageState = L"ready";
  session.hotspotStatus = L"stopped";
  session.hotspotSsid = L"LabHotspot";
  session.hotspotPassword = L"password123";
  session.wifiDirectAlias = L"ViewMesh-roomA";
  session.wifiAdapterPresent = true;
  session.hotspotSupported = true;
  session.wifiDirectApiAvailable = true;
  session.lastRooms = 1;
  session.lastViewers = 0;

  RuntimeHealthState health;
  health.serverProcessRunning = true;
  health.portReady = true;
  health.portDetail = L"listening";
  health.localHealthReady = true;
  health.localHealthDetail = L"ok";
  health.hostIpReachable = true;
  health.hostIpReachableDetail = L"reachable";
  health.lanBindReady = true;
  health.lanBindDetail = L"0.0.0.0";
  health.activeIpv4Candidates = 1;
  health.selectedIpRecommended = true;
  health.adapterHint = L"Wi-Fi";
  health.embeddedHostReady = true;
  health.embeddedHostStatus = L"ready";
  health.firewallReady = true;
  health.firewallDetail = L"allow rule detected";

  const auto report = BuildSelfCheckReport(session, health, true);
  Expect(report.total > 0, "self-check report should contain items");
  Expect(report.p0 == 0, "healthy runtime should not report P0 issues");
  Expect(BuildSelfCheckSummaryLine(report).find(L"ok") != std::wstring::npos,
         "summary line should mention ok count");

  const fs::path tempRoot = fs::temp_directory_path() / "lan_share_artifact_test";
  std::error_code ec;
  fs::remove_all(tempRoot, ec);
  fs::create_directories(tempRoot / "assets", ec);
  Expect(!ec, "temp output dir should be creatable");

  const fs::path qrAsset = tempRoot / "assets" / "share_card_qr.bundle.js";
  {
    std::ofstream qr(qrAsset, std::ios::binary);
    qr << "window.LanShareQr={renderInto:function(el){el.textContent='qr';},buildSvgMarkup:function(){return '<svg></svg>';}};";
  }

  ShareArtifactWriteRequest request;
  request.outputDir = tempRoot / "bundle";
  request.qrAssetSource = qrAsset;
  request.session = session;
  request.health = health;
  request.generatedAt = L"2026-03-13 17:00:00";

  ShareArtifactWriteResult result;
  Expect(ExportShareArtifacts(request, &result), "artifact export should succeed");
  Expect(fs::exists(result.shareCardPath), "share card should be written");
  Expect(fs::exists(result.shareWizardPath), "share wizard should be written");
  Expect(fs::exists(result.bundleJsonPath), "bundle json should be written");
  Expect(fs::exists(result.desktopSelfCheckPath), "desktop self-check html should be written");
  Expect(fs::exists(request.outputDir / "share_diagnostics.txt"), "diagnostics text should be written");
  Expect(fs::exists(request.outputDir / "www" / "assets" / "share_card_qr.bundle.js"), "qr asset should be copied");

  const std::string bundleJson = ReadFile(result.bundleJsonPath);
  Expect(bundleJson.find("share_status.js") != std::string::npos, "bundle json should reference live status script");
  Expect(bundleJson.find("192.168.1.20") != std::string::npos, "bundle json should include host ip");
  Expect(bundleJson.find("viewerUrl") != std::string::npos, "bundle json should include viewer url");

  const std::string diagnostics = ReadFile(request.outputDir / "share_diagnostics.txt");
  Expect(diagnostics.find("ViewMesh diagnostics") != std::string::npos,
         "diagnostics text should have heading");
  Expect(diagnostics.find("Operator first actions") != std::string::npos,
         "diagnostics text should include operator actions section");

  fs::remove_all(tempRoot, ec);
  std::cout << "share artifact service tests passed\n";
  return 0;
}
