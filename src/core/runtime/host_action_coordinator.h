#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace lan::runtime {

enum class HostActionKind {
  StartServer,
  StopServer,
  RestartServer,
  StartServiceOnly,
  StartAndOpenHost,
  OpenHostPage,
  OpenViewerPage,
  ShowQr,
  ShowShareWizard,
  ExportShareBundle,
  RunDesktopSelfCheck,
  RefreshDiagnosticsBundle,
  OpenDiagnosticsReport,
  OpenOutputFolder,
};

enum class HostActionPage {
  None,
  Setup,
  Network,
  Sharing,
  Diagnostics,
};

struct HostActionOperation {
  bool ok = false;
  bool performed = true;
  std::wstring detail;
};

struct HostActionArtifactRequest {
  bool shareCard = false;
  bool shareWizard = false;
  bool bundleJson = false;
  bool desktopSelfCheck = false;
};

struct HostActionArtifactPaths {
  std::filesystem::path shareCardPath;
  std::filesystem::path shareWizardPath;
  std::filesystem::path bundleJsonPath;
  std::filesystem::path desktopSelfCheckPath;
};

struct HostActionContext {
  std::filesystem::path outputDir;
  std::filesystem::path diagnosticsReportPath;
  bool diagnosticsReportExists = false;
};

struct HostActionEffects {
  bool handoffStarted = false;
  bool shareWizardOpened = false;
  bool shareCardExported = false;
  bool clearHandoffDelivered = false;
  bool refreshShareInfo = false;
  bool refreshDashboard = false;
  bool setPage = false;
  HostActionPage page = HostActionPage::None;
};

struct HostActionResult {
  bool ok = false;
  bool performed = false;
  std::vector<std::wstring> logs;
  std::vector<std::wstring> timelineEvents;
  HostActionEffects effects;
  HostActionArtifactPaths paths;
};

struct HostActionHooks {
  std::function<HostActionOperation()> startServer;
  std::function<HostActionOperation()> stopServer;
  std::function<HostActionOperation()> openHostPage;
  std::function<HostActionOperation()> openViewerPage;
  std::function<HostActionOperation(const HostActionArtifactRequest&, HostActionArtifactPaths&)> ensureArtifacts;
  std::function<HostActionOperation(const std::filesystem::path&)> openPath;
};

HostActionResult ExecuteHostAction(HostActionKind kind,
                                   const HostActionContext& context,
                                   const HostActionHooks& hooks);

} // namespace lan::runtime
