#include "platform/abstraction/platform_service_facade.h"
#include "platform/abstraction/system_actions.h"

#include <cassert>
#include <filesystem>
#include <memory>
#include <string>

namespace {

class FakeSystemActions final : public lan::platform::ISystemActions {
public:
  const char* ProviderName() const override {
    return "fake-system-actions";
  }

  bool OpenSystemPage(lan::platform::SystemPage page, std::string& err) override {
    lastPage = page;
    err.clear();
    ++openPageCalls;
    return pageOk;
  }

  bool OpenExternalUrl(std::string_view url, std::string& err) override {
    lastUrl = std::string(url);
    err.clear();
    ++openUrlCalls;
    return urlOk;
  }

  bool OpenExternalPath(const std::filesystem::path& path, std::string& err) override {
    lastPath = path;
    err.clear();
    ++openPathCalls;
    return pathOk;
  }

  bool ShowNotification(std::string_view title, std::string_view body, std::string& err) override {
    lastNotificationTitle = std::string(title);
    lastNotificationBody = std::string(body);
    err.clear();
    ++showNotificationCalls;
    return notificationOk;
  }

  lan::platform::SystemPage lastPage = lan::platform::SystemPage::ConnectedDevices;
  std::string lastUrl;
  std::filesystem::path lastPath;
  int openPageCalls = 0;
  int openUrlCalls = 0;
  int openPathCalls = 0;
  int showNotificationCalls = 0;
  std::string lastNotificationTitle;
  std::string lastNotificationBody;
  bool pageOk = true;
  bool urlOk = true;
  bool pathOk = true;
  bool notificationOk = true;
};

} // namespace

int main() {
  auto fake = std::make_unique<FakeSystemActions>();
  auto* raw = fake.get();
  lan::platform::PlatformServiceFacade facade(std::move(fake));

  std::string err;
  assert(std::string(facade.SystemProviderName()) == "fake-system-actions");

  assert(facade.OpenWifiDirectPairing(err));
  assert(raw->openPageCalls == 1);
  assert(raw->lastPage == lan::platform::SystemPage::ConnectedDevices);

  assert(facade.OpenSystemHotspotSettings(err));
  assert(raw->openPageCalls == 2);
  assert(raw->lastPage == lan::platform::SystemPage::MobileHotspot);

  assert(facade.OpenExternalUrl("https://example.test/view", err));
  assert(raw->openUrlCalls == 1);
  assert(raw->lastUrl == "https://example.test/view");

  const std::filesystem::path bundlePath = std::filesystem::path("out") / "share_bundle";
  assert(facade.OpenExternalPath(bundlePath, err));
  assert(raw->openPathCalls == 1);
  assert(raw->lastPath == bundlePath);

  assert(facade.ShowNotification("Tray", "Viewer connected", err));
  assert(raw->showNotificationCalls == 1);
  assert(raw->lastNotificationTitle == "Tray");
  assert(raw->lastNotificationBody == "Viewer connected");

  raw->pageOk = false;
  assert(!facade.OpenWifiDirectPairing(err));
  assert(raw->openPageCalls == 3);

  return 0;
}
