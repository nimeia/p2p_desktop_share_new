#include "host_shell/native_shell_action_controller.h"

#include "core/runtime/runtime_controller.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#if !defined(_WIN32)
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace lan::host_shell {
namespace {

std::string Narrow(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
}

lan::runtime::RuntimeSessionState BuildSessionState(const NativeShellActionConfig& config) {
  lan::runtime::RuntimeSessionState session;
  session.localeCode = config.localeCode;
  session.hostIp.assign(config.host.begin(), config.host.end());
  session.port = config.port;
  session.room = config.room;
  session.token = config.token;
  session.hostPageState = L"ready";
  return session;
}

NativeShellEndpointConfig BuildEndpointConfig(const NativeShellActionConfig& config) {
  NativeShellEndpointConfig endpoint;
  endpoint.host = config.host;
  endpoint.port = config.port;
  endpoint.timeoutMs = config.liveProbeRequestTimeoutMs;
  return endpoint;
}

std::string BuildLiveProbeError(std::string actionName, const NativeShellLiveSnapshot& snapshot) {
  std::ostringstream ss;
  ss << actionName << " did not reach the expected live state.";
  if (!snapshot.diagnostic.empty()) ss << " detail=" << snapshot.diagnostic;
  else if (!snapshot.runtime.detailText.empty()) ss << " detail=" << Narrow(snapshot.runtime.detailText);
  return ss.str();
}

bool ValidateDashboardSnapshot(const NativeShellLiveSnapshot& snapshot, std::string& err) {
  if (!snapshot.runtime.localHealthReady) {
    err = BuildLiveProbeError("Dashboard refresh", snapshot);
    return false;
  }
  if (!snapshot.statusEndpointReady) {
    err = BuildLiveProbeError("Dashboard refresh", snapshot);
    return false;
  }
  if (!snapshot.runtime.serverRunning) {
    err = "Dashboard refresh reports that the sharing service is not running.";
    return false;
  }
  err.clear();
  return true;
}

bool SpawnServerProcess(const NativeShellActionConfig& config, std::uint64_t& pidOut, std::string& err) {
  if (config.serverExecutable.empty()) {
    err = "Server executable is not configured.";
    return false;
  }
#if defined(_WIN32)
  (void)config;
  (void)pidOut;
  err = "Managed native shell server start is not implemented on Windows in this MVP.";
  return false;
#else
  std::vector<std::string> argvStorage;
  argvStorage.reserve(1 + config.serverArguments.size());
  argvStorage.push_back(config.serverExecutable.string());
  for (const auto& arg : config.serverArguments) argvStorage.push_back(arg);

  std::vector<char*> argv;
  argv.reserve(argvStorage.size() + 1);
  for (auto& item : argvStorage) argv.push_back(item.data());
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid < 0) {
    err = "Failed to fork managed sharing service process.";
    return false;
  }
  if (pid == 0) {
    setsid();
    execvp(argv[0], argv.data());
    _exit(127);
  }
  pidOut = static_cast<std::uint64_t>(pid);
  err.clear();
  return true;
#endif
}

bool TerminateServerProcess(std::uint64_t pid, std::string& err) {
#if defined(_WIN32)
  (void)pid;
  err = "Managed native shell server stop is not implemented on Windows in this MVP.";
  return false;
#else
  if (pid == 0) {
    err = "No managed sharing service process is running.";
    return false;
  }
  const pid_t nativePid = static_cast<pid_t>(pid);
  if (kill(nativePid, SIGTERM) != 0) {
    err = "Failed to signal managed sharing service process.";
    return false;
  }

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (std::chrono::steady_clock::now() < deadline) {
    int status = 0;
    const pid_t waitRc = waitpid(nativePid, &status, WNOHANG);
    if (waitRc == nativePid) {
      err.clear();
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  kill(nativePid, SIGKILL);
  int status = 0;
  (void)waitpid(nativePid, &status, 0);
  err.clear();
  return true;
#endif
}

} // namespace

NativeShellActionController::NativeShellActionController(NativeShellActionConfig config,
                                                         std::unique_ptr<lan::platform::PlatformServiceFacade> facade,
                                                         NativeShellPollFunction poll)
    : config_(std::move(config)),
      platformFacade_(facade ? std::move(facade) : lan::platform::CreateDefaultPlatformServiceFacade()),
      poll_(poll ? std::move(poll) : [endpoint = BuildEndpointConfig(config_)]() { return PollNativeShellLive(endpoint); }) {}

NativeShellActionController::~NativeShellActionController() {
  CleanupManagedServer();
}

NativeShellActionController::NativeShellActionController(NativeShellActionController&& other) noexcept
    : config_(std::move(other.config_)),
      platformFacade_(std::move(other.platformFacade_)),
      poll_(std::move(other.poll_)),
      managedServerPid_(other.managedServerPid_) {
  other.managedServerPid_ = 0;
}

NativeShellActionController& NativeShellActionController::operator=(NativeShellActionController&& other) noexcept {
  if (this == &other) return *this;
  CleanupManagedServer();
  config_ = std::move(other.config_);
  platformFacade_ = std::move(other.platformFacade_);
  poll_ = std::move(other.poll_);
  managedServerPid_ = other.managedServerPid_;
  other.managedServerPid_ = 0;
  return *this;
}

std::string NativeShellActionController::BuildDashboardUrl() const {
  return Narrow(lan::runtime::BuildHostUrl(BuildSessionState(config_)));
}

std::string NativeShellActionController::BuildViewerUrl() const {
  return Narrow(lan::runtime::BuildViewerUrl(BuildSessionState(config_)));
}

bool NativeShellActionController::OpenDashboard(std::string& err) {
  return platformFacade_->OpenExternalUrl(BuildDashboardUrl(), err);
}

bool NativeShellActionController::OpenViewer(std::string& err) {
  return platformFacade_->OpenExternalUrl(BuildViewerUrl(), err);
}

bool NativeShellActionController::OpenDiagnosticsFolder(std::string& err) {
  std::error_code ec;
  std::filesystem::create_directories(config_.diagnosticsDir, ec);
  return platformFacade_->OpenExternalPath(config_.diagnosticsDir, err);
}

NativeShellLiveSnapshot NativeShellActionController::PollLive() const {
  if (poll_) return poll_();
  return PollNativeShellLive(BuildEndpointConfig(config_));
}

bool NativeShellActionController::RefreshDashboard(std::string& err, NativeShellLiveSnapshot* snapshot) {
  const auto live = PollLive();
  if (snapshot) *snapshot = live;
  return ValidateDashboardSnapshot(live, err);
}

bool NativeShellActionController::WaitForLiveCondition(bool expectRunning,
                                                       std::string actionName,
                                                       std::string& err,
                                                       NativeShellLiveSnapshot* snapshot) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(config_.liveProbeTimeoutMs);
  NativeShellLiveSnapshot last;
  while (std::chrono::steady_clock::now() <= deadline) {
    last = PollLive();
    const bool ready = expectRunning
                           ? (last.runtime.localHealthReady && last.statusEndpointReady && last.runtime.serverRunning)
                           : (!last.runtime.localHealthReady && !last.statusEndpointReady && !last.runtime.serverRunning);
    if (ready) {
      if (snapshot) *snapshot = last;
      err.clear();
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(std::max(10, config_.liveProbeIntervalMs)));
  }

  if (snapshot) *snapshot = last;
  err = BuildLiveProbeError(std::move(actionName), last);
  return false;
}

bool NativeShellActionController::StartServer(std::string& err, NativeShellLiveSnapshot* snapshot) {
  if (managedServerPid_ != 0) {
    err = "A managed sharing service process is already running.";
    return false;
  }
  if (!SpawnServerProcess(config_, managedServerPid_, err)) return false;
  if (WaitForLiveCondition(true, "Start sharing service", err, snapshot)) return true;
  CleanupManagedServer();
  return false;
}

bool NativeShellActionController::StopServer(std::string& err, NativeShellLiveSnapshot* snapshot) {
  if (managedServerPid_ == 0) {
    err = "No managed sharing service process is running.";
    return false;
  }
  if (!TerminateServerProcess(managedServerPid_, err)) return false;
  managedServerPid_ = 0;
  return WaitForLiveCondition(false, "Stop sharing service", err, snapshot);
}

bool NativeShellActionController::IsServerRunning() const {
  if (managedServerPid_ == 0) return false;
#if defined(_WIN32)
  return true;
#else
  const pid_t nativePid = static_cast<pid_t>(managedServerPid_);
  if (kill(nativePid, 0) == 0) return true;
  return false;
#endif
}

bool NativeShellActionController::RevealDiagnosticsExport(const std::filesystem::path& exportedPath, std::string& err) {
  switch (config_.diagnosticsRevealMode) {
  case DiagnosticsExportRevealMode::None:
    err.clear();
    return true;
  case DiagnosticsExportRevealMode::Folder:
    return platformFacade_->OpenExternalPath(exportedPath.parent_path(), err);
  case DiagnosticsExportRevealMode::File:
    return platformFacade_->OpenExternalPath(exportedPath, err);
  case DiagnosticsExportRevealMode::FileThenFolder:
    if (platformFacade_->OpenExternalPath(exportedPath, err)) return true;
    return platformFacade_->OpenExternalPath(exportedPath.parent_path(), err);
  }
  err = "Unknown diagnostics reveal mode.";
  return false;
}

bool NativeShellActionController::ExportDiagnostics(const std::wstring& statusText,
                                                    const std::wstring& detailText,
                                                    const std::wstring& badgeText,
                                                    std::string& err,
                                                    std::filesystem::path* exportedPath) {
  std::error_code ec;
  std::filesystem::create_directories(config_.diagnosticsDir, ec);
  if (ec) {
    err = ec.message();
    return false;
  }

  const auto now = std::chrono::system_clock::now();
  const auto nowT = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &nowT);
#else
  localtime_r(&nowT, &tm);
#endif
  std::ostringstream name;
  name << "native_shell_diag_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".txt";
  const auto path = config_.diagnosticsDir / name.str();

  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    err = "Failed to create diagnostics export file.";
    return false;
  }
  out << "dashboard_url=" << BuildDashboardUrl() << "\n";
  out << "viewer_url=" << BuildViewerUrl() << "\n";
  out << "status=" << Narrow(statusText) << "\n";
  out << "detail=" << Narrow(detailText) << "\n";
  out << "badge=" << Narrow(badgeText) << "\n";
  out << "diagnostics_dir=" << config_.diagnosticsDir.string() << "\n";
  out.close();
  if (!out) {
    err = "Failed to flush diagnostics export file.";
    return false;
  }

  if (exportedPath) *exportedPath = path;
  if (!RevealDiagnosticsExport(path, err)) return false;
  err.clear();
  return true;
}

void NativeShellActionController::CleanupManagedServer() {
  if (managedServerPid_ == 0) return;
  std::string err;
  (void)TerminateServerProcess(managedServerPid_, err);
  managedServerPid_ = 0;
}

} // namespace lan::host_shell
