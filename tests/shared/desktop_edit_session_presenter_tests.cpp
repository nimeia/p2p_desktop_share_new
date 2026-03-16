#include "core/runtime/desktop_edit_session_presenter.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "desktop edit session presenter test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  HostSessionState applied;
  applied.rules.defaultPort = 9443;
  applied.rules.defaultBindAddress = L"0.0.0.0";
  applied.config.bindAddress = L"0.0.0.0";
  applied.config.port = 9443;
  applied.config.room = L"roomA";
  applied.config.token = L"tokenA";

  DesktopEditSessionDraft sameDraft = BuildDesktopEditSessionDraft(applied, 0);
  DesktopEditSessionInput sameInput;
  sameInput.appliedState = applied;
  sameInput.draft = sameDraft;
  sameInput.serverRunning = false;
  sameInput.hostIp = L"192.168.1.10";
  sameInput.outputDir = L"C:/out";
  sameInput.shareBundleDir = L"C:/out/share_bundle";

  auto sameView = BuildDesktopEditSessionViewModel(sameInput);
  Expect(!sameView.dirty, "matching draft should not be dirty");
  Expect(!sameView.pendingApply, "matching draft should not be pending apply");
  Expect(sameView.sessionSummaryLabel == L"Live session summary before launch:", "matching draft should keep live summary label");
  Expect(sameView.startButtonLabel == L"Start", "matching draft should keep default start label");

  DesktopEditSessionDraft changedDraft = sameDraft;
  changedDraft.room = L"roomB";
  auto changedMutation = ApplyDesktopEditSessionDraft(applied, changedDraft, true);
  Expect(changedMutation.configChanged, "changed draft should mark config changed");
  Expect(changedMutation.state.config.room == L"roomB", "changed draft should update room");
  Expect(changedMutation.resetDeliveryStateApplied, "reset flag should be applied when requested");

  DesktopEditSessionInput runningInput = sameInput;
  runningInput.serverRunning = true;
  runningInput.draft = changedDraft;
  auto runningView = BuildDesktopEditSessionViewModel(runningInput);
  Expect(runningView.dirty, "changed draft should be dirty");
  Expect(runningView.pendingApply, "changed draft while running should require apply");
  Expect(runningView.restartButtonLabel == L"Restart To Apply", "running dirty draft should retitle restart");
  Expect(!runningView.buttonPolicy.startAndOpenHostEnabled, "running dirty draft should disable start+open");
  Expect(!runningView.buttonPolicy.serviceOnlyEnabled, "running dirty draft should disable service only");

  DesktopEditSessionDraft invalidPort = sameDraft;
  invalidPort.portText = L"70000";
  auto invalidView = BuildDesktopEditSessionViewModel({applied, invalidPort, false, L"192.168.1.10", L"C:/out", L"C:/out/share_bundle"});
  Expect(!invalidView.portValid, "invalid port should be flagged");
  Expect(invalidView.dirty, "invalid port should keep draft dirty");

  const auto fixedRoomDraft = ApplyDesktopSessionTemplate(applied, static_cast<int>(DesktopSessionTemplateKind::FixedRoom));
  Expect(fixedRoomDraft.room == L"meeting-room", "fixed room template should set room");
  Expect(fixedRoomDraft.token == L"persistent-token", "fixed room template should set token");

  const auto demoDraft = ApplyDesktopSessionTemplate(applied, static_cast<int>(DesktopSessionTemplateKind::DemoMode));
  Expect(demoDraft.room == L"demo", "demo template should set room");
  Expect(demoDraft.token == L"demo-view", "demo template should set token");

  std::cout << "desktop edit session presenter tests passed\n";
  return 0;
}
