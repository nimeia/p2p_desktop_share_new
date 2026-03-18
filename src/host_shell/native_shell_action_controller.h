#pragma once

#include "host_shell/native_shell_live_poller.h"
#include "platform/abstraction/platform_service_facade.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lan::host_shell {

using NativeShellPollFunction = std::function<NativeShellLiveSnapshot()>;

enum class DiagnosticsExportRevealMode {
  None,
  Folder,
  File,
  FileThenFolder,
};

struct NativeShellActionConfig {
  std::string host = "127.0.0.1";
  int port = 8443;
  std::wstring room = L"demo";
  std::wstring token = L"demo";
  std::wstring localeCode;
  std::filesystem::path diagnosticsDir = std::filesystem::current_path() / "out" / "diagnostics";
  std::filesystem::path serverExecutable;
  std::vector<std::string> serverArguments;
  int liveProbeTimeoutMs = 5000;
  int liveProbeIntervalMs = 250;
  int liveProbeRequestTimeoutMs = 1500;
  DiagnosticsExportRevealMode diagnosticsRevealMode = DiagnosticsExportRevealMode::Folder;
};

class NativeShellActionController {
public:
  explicit NativeShellActionController(
      NativeShellActionConfig config,
      std::unique_ptr<lan::platform::PlatformServiceFacade> facade = lan::platform::CreateDefaultPlatformServiceFacade(),
      NativeShellPollFunction poll = {});
  ~NativeShellActionController();

  NativeShellActionController(const NativeShellActionController&) = delete;
  NativeShellActionController& operator=(const NativeShellActionController&) = delete;
  NativeShellActionController(NativeShellActionController&& other) noexcept;
  NativeShellActionController& operator=(NativeShellActionController&& other) noexcept;

  lan::platform::PlatformServiceFacade& Platform() { return *platformFacade_; }
  const NativeShellActionConfig& Config() const { return config_; }

  std::string BuildDashboardUrl() const;
  std::string BuildViewerUrl() const;

  bool OpenDashboard(std::string& err);
  bool OpenViewer(std::string& err);
  bool OpenDiagnosticsFolder(std::string& err);
  bool RefreshDashboard(std::string& err, NativeShellLiveSnapshot* snapshot = nullptr);
  bool StartServer(std::string& err, NativeShellLiveSnapshot* snapshot = nullptr);
  bool StopServer(std::string& err, NativeShellLiveSnapshot* snapshot = nullptr);
  bool IsServerRunning() const;
  bool ExportDiagnostics(const std::wstring& statusText,
                         const std::wstring& detailText,
                         const std::wstring& badgeText,
                         std::string& err,
                         std::filesystem::path* exportedPath = nullptr);

private:
  NativeShellLiveSnapshot PollLive() const;
  bool WaitForLiveCondition(bool expectRunning,
                            std::string actionName,
                            std::string& err,
                            NativeShellLiveSnapshot* snapshot);
  bool RevealDiagnosticsExport(const std::filesystem::path& exportedPath, std::string& err);
  void CleanupManagedServer();

  NativeShellActionConfig config_;
  std::unique_ptr<lan::platform::PlatformServiceFacade> platformFacade_;
  NativeShellPollFunction poll_;
  std::uint64_t managedServerPid_ = 0;
};

} // namespace lan::host_shell
