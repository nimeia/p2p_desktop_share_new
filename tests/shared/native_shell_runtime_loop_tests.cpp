#include "host_shell/native_shell_runtime_loop.h"
#include "platform/abstraction/platform_service_facade.h"
#include "platform/abstraction/system_actions.h"

#include <cassert>
#include <memory>
#include <string>

namespace {

class FakeSystemActions final : public lan::platform::ISystemActions {
public:
  const char* ProviderName() const override { return "fake-shell-actions"; }
  bool OpenSystemPage(lan::platform::SystemPage, std::string& err) override {
    err.clear();
    return true;
  }
  bool OpenExternalUrl(std::string_view, std::string& err) override {
    err.clear();
    return true;
  }
  bool OpenExternalPath(const std::filesystem::path&, std::string& err) override {
    err.clear();
    return true;
  }
  bool ShowNotification(std::string_view title, std::string_view body, std::string& err) override {
    lastTitle = std::string(title);
    lastBody = std::string(body);
    ++notifications;
    err.clear();
    return true;
  }

  int notifications = 0;
  std::string lastTitle;
  std::string lastBody;
};

} // namespace

int main() {
  auto fake = std::make_unique<FakeSystemActions>();
  auto* raw = fake.get();
  lan::platform::PlatformServiceFacade facade(std::move(fake));

  int tickCount = 0;
  auto poll = [&]() {
    lan::host_shell::NativeShellLiveSnapshot snapshot;
    snapshot.runtime.serverRunning = true;
    snapshot.runtime.localHealthReady = tickCount >= 1;
    snapshot.runtime.viewerCount = tickCount >= 3 ? 2 : 0;
    snapshot.runtime.hostPageState = snapshot.runtime.viewerCount > 0 ? L"sharing" : L"ready";
    snapshot.runtime.detailText = snapshot.runtime.localHealthReady ? L"Healthy" : L"Probe failing";
    ++tickCount;
    return snapshot;
  };

  lan::runtime::NativeShellAlertDebounceConfig debounce;
  debounce.healthStableSamples = 2;
  debounce.viewerStableSamples = 2;
  debounce.notificationCooldownTicks = 0;

  lan::host_shell::NativeShellRuntimeLoop loop(poll, facade, debounce);
  auto tick = loop.Tick();
  assert(raw->notifications == 0);
  assert(tick.tracker.statusViewModel.statusText == L"Status: running");

  tick = loop.Tick();
  assert(raw->notifications == 0);

  tick = loop.Tick();
  assert(raw->notifications == 1);
  assert(raw->lastTitle == "Sharing service recovered");

  tick = loop.Tick();
  assert(raw->notifications == 1);

  tick = loop.Tick();
  assert(raw->notifications == 2);
  assert(raw->lastTitle == "Viewer count changed");
  assert(tick.tracker.trayIconViewModel.statusBadge == L"2 viewer(s)");

  return 0;
}
