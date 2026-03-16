#include "core/runtime/admin_shell_runtime_publisher.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "admin shell runtime publisher test failed: " << message << "\n";
    std::exit(1);
  }
}

lan::runtime::AdminViewModelInput BuildInput() {
  lan::runtime::AdminViewModelInput input;
  input.appName = L"LanScreenShareHostApp";
  input.nativePage = L"dashboard";
  input.runtimeSnapshot.session.bindAddress = L"0.0.0.0";
  input.runtimeSnapshot.session.port = 9443;
  input.runtimeSnapshot.session.hostIp = L"192.168.1.5";
  input.runtimeSnapshot.session.room = L"room-a";
  input.runtimeSnapshot.session.token = L"token-a";
  input.runtimeSnapshot.hostUrl = L"https://192.168.1.5:9443/host";
  input.runtimeSnapshot.viewerUrl = L"https://192.168.1.5:9443/viewer";
  input.runtimeSnapshot.session.lastRooms = 1;
  input.runtimeSnapshot.session.lastViewers = 2;
  input.runtimeSnapshot.health.serverProcessRunning = true;
  input.runtimeSnapshot.health.hostIpReachable = true;
  input.runtimeSnapshot.health.certReady = true;
  input.runtimeSnapshot.handoff.state = L"ready";
  input.runtimeSnapshot.handoff.label = L"Ready";
  input.runtimeSnapshot.handoff.detail = L"Viewer link can be shared.";
  input.sessionModel.defaultBindAddress = L"0.0.0.0";
  input.sessionModel.defaultPort = 9443;
  input.outputDir = L"C:/out";
  input.bundleDir = L"C:/out/share_bundle";
  input.serverExePath = L"C:/app/lan_screenshare_server.exe";
  input.certDir = L"C:/app/cert";
  input.logTail = L"tail";
  input.timelineText = L"timeline";
  input.networkCandidates.push_back({L"Wi-Fi", L"192.168.1.5", L"wireless", true, true, true, L"LAN /health ok", L"Selected adapter answered"});
  return input;
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto requestPolicy = ResolveAdminShellRuntimeRefreshPolicy(true, false);
  Expect(requestPolicy.markShellReady, "request-snapshot should mark shell ready");
  Expect(requestPolicy.shouldPublish, "request-snapshot should publish");

  const auto statePolicy = ResolveAdminShellRuntimeRefreshPolicy(false, true);
  Expect(!statePolicy.markShellReady, "state change should not mark shell ready");
  Expect(statePolicy.shouldPublish, "state change should publish");

  const auto idlePolicy = ResolveAdminShellRuntimeRefreshPolicy(false, false);
  Expect(!idlePolicy.shouldPublish, "idle policy should not publish");

  const auto snapshot = BuildAdminShellSnapshotState(BuildInput());
  Expect(snapshot.nativePage == L"dashboard", "snapshot page should be preserved");
  Expect(snapshot.hostIp == L"192.168.1.5", "snapshot host ip should be preserved");
  Expect(snapshot.rooms == 1 && snapshot.viewers == 2, "snapshot room/viewer counts should be preserved");
  Expect(snapshot.networkCandidates.size() == 1, "snapshot network candidates should be mapped");
  Expect(snapshot.networkCandidates.front().recommended, "snapshot candidate flags should be mapped");

  std::wstring publishedJson;
  AdminShellRuntimePublishContext context;
  context.adminShellActive = true;
  context.adminShellReady = true;
  context.viewModelInput = BuildInput();

  AdminShellRuntimePublisherHooks hooks;
  hooks.publishJson = [&](const std::wstring& json) { publishedJson = json; };

  const auto publishResult = PublishAdminShellRuntime(context, hooks);
  Expect(publishResult.published, "active ready shell should publish");
  Expect(!publishedJson.empty(), "publisher hook should receive json");
  Expect(publishedJson.find(L"\"name\":\"state.snapshot\"") != std::wstring::npos,
         "published json should contain snapshot event name");
  Expect(publishedJson.find(L"\"hostIp\":\"192.168.1.5\"") != std::wstring::npos,
         "published json should contain host ip");

  AdminShellRuntimePublishContext inactiveContext = context;
  inactiveContext.adminShellActive = false;
  publishedJson.clear();
  const auto inactiveResult = PublishAdminShellRuntime(inactiveContext, hooks);
  Expect(!inactiveResult.published, "inactive shell should not publish");
  Expect(publishedJson.empty(), "inactive shell should not invoke publish hook");

  std::cout << "admin shell runtime publisher tests passed\n";
  return 0;
}
