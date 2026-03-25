#include "host_shell/native_shell_action_controller.h"
#include "platform/abstraction/platform_service_facade.h"
#include "platform/abstraction/system_actions.h"

#include <cassert>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

class FakeSystemActions final : public lan::platform::ISystemActions {
public:
  const char* ProviderName() const override { return "fake-system-actions"; }
  bool OpenSystemPage(lan::platform::SystemPage, std::string& err) override {
    err.clear();
    return true;
  }
  bool OpenExternalUrl(std::string_view url, std::string& err) override {
    lastUrl = std::string(url);
    ++openUrlCalls;
    err.clear();
    return true;
  }
  bool OpenExternalPath(const std::filesystem::path& path, std::string& err) override {
    openedPaths.push_back(path);
    lastPath = path;
    ++openPathCalls;
    err.clear();
    return true;
  }
  bool ShowNotification(std::string_view, std::string_view, std::string& err) override {
    err.clear();
    return true;
  }

  std::string lastUrl;
  std::filesystem::path lastPath;
  std::vector<std::filesystem::path> openedPaths;
  int openUrlCalls = 0;
  int openPathCalls = 0;
};

lan::host_shell::NativeShellLiveSnapshot MakeLive(bool running, bool healthReady, bool statusReady, std::size_t viewers = 0) {
  lan::host_shell::NativeShellLiveSnapshot snapshot;
  snapshot.runtime.serverRunning = running;
  snapshot.runtime.localHealthReady = healthReady;
  snapshot.runtime.viewerCount = viewers;
  snapshot.runtime.hostPageState = running ? (viewers > 0 ? L"sharing" : L"ready") : L"stopped";
  snapshot.runtime.detailText = running ? L"Waiting for viewers." : L"Stopped.";
  snapshot.runtime.attentionNeeded = !running || !healthReady;
  snapshot.statusEndpointReady = statusReady;
  snapshot.viewers = viewers;
  return snapshot;
}

std::filesystem::path WriteSleepScript() {
  const auto path = std::filesystem::temp_directory_path() / "lan_native_shell_sleep.sh";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "#!/bin/sh\n";
  out << "sleep 30\n";
  out.close();
  std::filesystem::permissions(path,
                               std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write |
                                   std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::group_read |
                                   std::filesystem::perms::group_exec,
                               std::filesystem::perm_options::replace);
  return path;
}

} // namespace

int main() {
  {
    auto fake = std::make_unique<FakeSystemActions>();
    auto* raw = fake.get();

    lan::host_shell::NativeShellActionConfig config;
    config.host = "10.0.0.12";
    config.port = 9443;
    config.room = L"roomA";
    config.token = L"tokA";
    config.diagnosticsDir = std::filesystem::temp_directory_path() / "lan_native_shell_diag_tests";

    auto facade = std::make_unique<lan::platform::PlatformServiceFacade>(std::move(fake));
    lan::host_shell::NativeShellActionController controller(std::move(config), std::move(facade));

    const auto dashboardUrl = controller.BuildDashboardUrl();
    const auto viewerUrl = controller.BuildViewerUrl();
    assert(dashboardUrl == "http://127.0.0.1:9443/host?room=roomA&token=tokA&lang=en");
    assert(viewerUrl == "http://10.0.0.12:9443/view?room=roomA&token=tokA&lang=en");

    std::string err;
    assert(controller.OpenDashboard(err));
    assert(raw->openUrlCalls == 1);
    assert(raw->lastUrl.find("/host?") != std::string::npos);

    assert(controller.OpenViewer(err));
    assert(raw->openUrlCalls == 2);
    assert(raw->lastUrl.find("/view?") != std::string::npos);

    assert(controller.OpenDiagnosticsFolder(err));
    assert(raw->openPathCalls == 1);
    assert(raw->lastPath == controller.Config().diagnosticsDir);
  }

  {
    auto fake = std::make_unique<FakeSystemActions>();
    auto* raw = fake.get();
    lan::host_shell::NativeShellActionConfig config;
    config.diagnosticsDir = std::filesystem::temp_directory_path() / "lan_native_shell_diag_reveal_tests";
    config.diagnosticsRevealMode = lan::host_shell::DiagnosticsExportRevealMode::File;

    auto facade = std::make_unique<lan::platform::PlatformServiceFacade>(std::move(fake));
    lan::host_shell::NativeShellActionController controller(std::move(config), std::move(facade));

    std::string err;
    std::filesystem::path exported;
    assert(controller.ExportDiagnostics(L"Status: running", L"Waiting for viewers.", L"Ready", err, &exported));
    assert(std::filesystem::exists(exported));
    assert(raw->openPathCalls == 1);
    assert(raw->lastPath == exported);
  }

  {
    auto fake = std::make_unique<FakeSystemActions>();
    lan::host_shell::NativeShellActionConfig config;
    config.liveProbeTimeoutMs = 20;
    config.liveProbeIntervalMs = 1;

    std::deque<lan::host_shell::NativeShellLiveSnapshot> samples;
    samples.push_back(MakeLive(false, false, false));
    auto poll = [&]() {
      auto snapshot = samples.empty() ? MakeLive(false, false, false) : samples.front();
      if (!samples.empty()) samples.pop_front();
      return snapshot;
    };

    auto facade = std::make_unique<lan::platform::PlatformServiceFacade>(std::move(fake));
    lan::host_shell::NativeShellActionController controller(std::move(config), std::move(facade), poll);
    std::string err;
    lan::host_shell::NativeShellLiveSnapshot live;
    assert(!controller.RefreshDashboard(err, &live));
    assert(!err.empty());
    assert(!live.runtime.localHealthReady);
  }

#if !defined(_WIN32)
  {
    auto fake = std::make_unique<FakeSystemActions>();
    lan::host_shell::NativeShellActionConfig config;
    config.serverExecutable = WriteSleepScript();
    config.liveProbeTimeoutMs = 50;
    config.liveProbeIntervalMs = 1;
    config.liveProbeRequestTimeoutMs = 10;

    std::deque<lan::host_shell::NativeShellLiveSnapshot> samples;
    samples.push_back(MakeLive(false, false, false));
    samples.push_back(MakeLive(true, true, true));
    samples.push_back(MakeLive(false, false, false));
    auto poll = [&]() {
      auto snapshot = samples.empty() ? MakeLive(false, false, false) : samples.front();
      if (!samples.empty()) samples.pop_front();
      return snapshot;
    };

    auto facade = std::make_unique<lan::platform::PlatformServiceFacade>(std::move(fake));
    lan::host_shell::NativeShellActionController controller(std::move(config), std::move(facade), poll);

    std::string err;
    lan::host_shell::NativeShellLiveSnapshot live;
    assert(controller.StartServer(err, &live));
    assert(controller.IsServerRunning());
    assert(live.runtime.localHealthReady);

    assert(controller.StopServer(err, &live));
    assert(!controller.IsServerRunning());
    assert(!live.runtime.localHealthReady);
  }
#endif

  return 0;
}
