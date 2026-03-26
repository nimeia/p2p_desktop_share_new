#import <Cocoa/Cocoa.h>

#include "core/i18n/localization.h"
#include "host_shell/native_shell_action_controller.h"
#include "host_shell/native_shell_runtime_loop.h"
#include "platform/abstraction/runtime_paths.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace {

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

std::wstring WideAscii(std::string_view value) {
  std::wstring out;
  out.reserve(value.size());
  for (unsigned char ch : value) out.push_back(static_cast<wchar_t>(ch));
  return out;
}

std::wstring CurrentLocaleCode() {
  return lan::i18n::LoadPreferredLocale();
}

NSString* LocalizedWide(std::wstring_view source) {
  return ToNSString(lan::i18n::TranslateNativeTextUtf8(source, CurrentLocaleCode()));
}

NSString* LocalizedAscii(std::string_view source) {
  for (unsigned char ch : source) {
    if (ch >= 0x80) return ToNSString(std::string(source));
  }
  return ToNSString(lan::i18n::TranslateNativeTextUtf8(WideAscii(source), CurrentLocaleCode()));
}

std::string Narrow(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
}

NSString* ViewerTitle(std::size_t viewers) {
  if (viewers == 0) return LocalizedWide(L"Open Viewer URL");
  if (viewers == 1) return LocalizedWide(L"Open Viewer URL (1 viewer)");
  return LocalizedWide(L"Open Viewer URL (" + std::to_wstring(viewers) + L" viewers)");
}

std::filesystem::path ResolveMacSupportDir() {
  @autoreleasepool {
    NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    if (paths.count > 0) {
      return std::filesystem::path([[paths objectAtIndex:0] UTF8String]) / "ViewMesh";
    }
  }

  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path(home) / "Library" / "Application Support" / "ViewMesh";
  }
  return std::filesystem::current_path() / "ViewMesh";
}

std::filesystem::path ResolveMacDefaultServerExecutable(const std::filesystem::path& executableDir) {
  const std::filesystem::path candidates[] = {
      executableDir / "ViewMeshServer",
      executableDir / "lan_screenshare_server",
      executableDir.parent_path() / "Resources" / "runtime" / "ViewMeshServer",
      executableDir.parent_path() / "Resources" / "runtime" / "lan_screenshare_server",
      executableDir.parent_path().parent_path().parent_path() / "ViewMeshServer",
      executableDir.parent_path().parent_path().parent_path() / "lan_screenshare_server",
      executableDir.parent_path().parent_path().parent_path() / "server" / "ViewMeshServer",
      executableDir.parent_path().parent_path().parent_path() / "server" / "lan_screenshare_server",
  };

  for (const auto& candidate : candidates) {
    if (!candidate.empty() && std::filesystem::exists(candidate)) return candidate;
  }
  return {};
}

NSString* MenuBarIconName(bool attention, std::size_t viewers, bool sharing) {
  if (attention) return @"statusbar_alertTemplate";
  if (viewers > 0) return @"statusbar_connectedTemplate";
  if (sharing) return @"statusbar_sharingTemplate";
  return @"statusbarTemplate";
}

NSImage* LoadMenuBarImage(NSString* name) {
  if (!name.length) return nil;

  NSImage* image = [NSImage imageNamed:name];
  if (!image) {
    NSString* path = [[NSBundle mainBundle] pathForResource:name ofType:@"png"];
    if (path) {
      image = [[NSImage alloc] initWithContentsOfFile:path];
    }
  }

  if (image) {
    image.template = YES;
  }
  return image;
}

NSStatusBarButton* StatusItemButton(NSStatusItem* statusItem) {
  return statusItem ? [statusItem button] : nil;
}

struct LanMenuBarCppState {
  std::unique_ptr<lan::host_shell::NativeShellActionController> controller;
  std::unique_ptr<lan::host_shell::NativeShellRuntimeLoop> loop;
  lan::host_shell::NativeShellRuntimeLoopResult lastTick;
  bool hasLastTick = false;

  LanMenuBarCppState(std::unique_ptr<lan::host_shell::NativeShellActionController> controllerIn,
                     std::unique_ptr<lan::host_shell::NativeShellRuntimeLoop> loopIn)
      : controller(std::move(controllerIn)), loop(std::move(loopIn)) {}
};

} // namespace

@interface LanMenuBarController : NSObject {
 @private
  NSStatusItem* _statusItem;
  NSMenuItem* _statusLineItem;
  NSMenuItem* _detailLineItem;
  NSMenuItem* _openDashboardItem;
  NSMenuItem* _refreshDashboardItem;
  NSMenuItem* _openViewerItem;
  NSMenuItem* _startServerItem;
  NSMenuItem* _stopServerItem;
  NSMenuItem* _openDiagnosticsItem;
  NSMenuItem* _exportDiagnosticsItem;
  NSTimer* _timer;
  LanMenuBarCppState* _state;
}
- (instancetype)init;
- (void)bootstrapWithState:(void*)state;
- (void)notifyTitleText:(NSString*)title bodyText:(NSString*)body;
- (void)applyCurrentTick;
- (void)refresh:(id)sender;
- (void)openDashboard:(id)sender;
- (void)refreshDashboard:(id)sender;
- (void)openViewer:(id)sender;
- (void)startServer:(id)sender;
- (void)stopServer:(id)sender;
- (void)openDiagnostics:(id)sender;
- (void)exportDiagnostics:(id)sender;
- (void)quit:(id)sender;
@end

@implementation LanMenuBarController

- (instancetype)init {
  self = [super init];
  if (!self) return nil;

  _state = nullptr;
  _timer = nil;

  _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  NSStatusBarButton* button = StatusItemButton(_statusItem);
  if (button != nil) {
    [button setImagePosition:NSImageOnly];
    [button setImage:LoadMenuBarImage(MenuBarIconName(false, 0, false))];
    [button setTitle:(button.image ? @"" : @"LAN Share")];
    [button setToolTip:LocalizedWide(L"ViewMesh Host")];
  }

  NSMenu* menu = [[NSMenu alloc] initWithTitle:LocalizedWide(L"ViewMesh Host")];
  _statusLineItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Status: starting") action:NULL keyEquivalent:@""];
  _detailLineItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Waiting for first refresh...") action:NULL keyEquivalent:@""];
  _statusLineItem.enabled = NO;
  _detailLineItem.enabled = NO;
  [menu addItem:_statusLineItem];
  [menu addItem:_detailLineItem];
  [menu addItem:[NSMenuItem separatorItem]];

  _openDashboardItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Open Dashboard") action:@selector(openDashboard:) keyEquivalent:@""];
  _openDashboardItem.target = self;
  [menu addItem:_openDashboardItem];

  _refreshDashboardItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Refresh Dashboard") action:@selector(refreshDashboard:) keyEquivalent:@""];
  _refreshDashboardItem.target = self;
  _refreshDashboardItem.enabled = NO;
  [menu addItem:_refreshDashboardItem];

  _openViewerItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Open Viewer URL") action:@selector(openViewer:) keyEquivalent:@""];
  _openViewerItem.target = self;
  _openViewerItem.enabled = NO;
  [menu addItem:_openViewerItem];

  _startServerItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Start Sharing Service") action:@selector(startServer:) keyEquivalent:@""];
  _startServerItem.target = self;
  _startServerItem.enabled = NO;
  [menu addItem:_startServerItem];

  _stopServerItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Stop Sharing Service") action:@selector(stopServer:) keyEquivalent:@""];
  _stopServerItem.target = self;
  _stopServerItem.enabled = NO;
  [menu addItem:_stopServerItem];

  _openDiagnosticsItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Open Diagnostics Folder") action:@selector(openDiagnostics:) keyEquivalent:@""];
  _openDiagnosticsItem.target = self;
  _openDiagnosticsItem.enabled = NO;
  [menu addItem:_openDiagnosticsItem];

  _exportDiagnosticsItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Export Diagnostics Snapshot") action:@selector(exportDiagnostics:) keyEquivalent:@""];
  _exportDiagnosticsItem.target = self;
  _exportDiagnosticsItem.enabled = NO;
  [menu addItem:_exportDiagnosticsItem];

  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:LocalizedWide(L"Quit") action:@selector(quit:) keyEquivalent:@"q"];
  quitItem.target = self;
  [menu addItem:quitItem];

  _statusItem.menu = menu;
  return self;
}

- (void)bootstrapWithState:(void*)state {
  delete _state;
  _state = static_cast<LanMenuBarCppState*>(state);
  if (_timer != nil) {
    [_timer invalidate];
    _timer = nil;
  }
  [self refresh:nil];
  _timer = [NSTimer scheduledTimerWithTimeInterval:2.0 target:self selector:@selector(refresh:) userInfo:nil repeats:YES];
}

- (void)dealloc {
  if (_timer != nil) {
    [_timer invalidate];
    _timer = nil;
  }
  if (_statusItem != nil) {
    [[NSStatusBar systemStatusBar] removeStatusItem:_statusItem];
  }
  delete _state;
  _state = nullptr;
#if !__has_feature(objc_arc)
  [super dealloc];
#endif
}

- (void)notifyTitleText:(NSString*)title bodyText:(NSString*)body {
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  const std::string titleUtf8 = title ? std::string([title UTF8String]) : std::string();
  const std::string bodyUtf8 = body ? std::string([body UTF8String]) : std::string();
  if (!_state->controller->Platform().ShowNotification(titleUtf8, bodyUtf8, err) && !err.empty()) {
    NSLog(@"Notification error: %s", err.c_str());
  }
}

- (void)applyCurrentTick {
  if (_state == nullptr || !_state->hasLastTick) return;
  const auto& tick = _state->lastTick;
  auto* controller = (_state != nullptr && _state->controller) ? _state->controller.get() : nullptr;
  const auto status = LocalizedWide(tick.tracker.statusViewModel.statusText);
  const bool stableRunning = tick.tracker.memory.stableServerRunning;
  const bool stableHealthy = tick.tracker.memory.stableHealthReady;
  const bool attention = tick.tracker.chromeInput.attentionNeeded;
  const auto viewers = tick.tracker.memory.stableViewerCount;
  const bool sharing = stableRunning && tick.tracker.chromeInput.hostStateSharing;
  const bool canRefreshDashboard = stableRunning;
  const bool canOpenViewer = stableRunning && stableHealthy && tick.tracker.trayMenuViewModel.copyViewerUrlEnabled;
  const bool canStartServer = !stableRunning;
  const bool canStopServer = stableRunning || (controller != nullptr && controller->IsServerRunning());
  const bool canOpenDiagnostics = stableRunning || attention;
  const bool canExportDiagnostics = canOpenDiagnostics || viewers > 0;

  std::string detail = lan::i18n::TranslateNativeTextUtf8(tick.tracker.statusViewModel.detailText, CurrentLocaleCode());
  if (stableRunning) {
    detail += stableHealthy ? std::string(" | ") + lan::i18n::TranslateNativeTextUtf8(L"Healthy", CurrentLocaleCode())
                            : std::string(" | ") + lan::i18n::TranslateNativeTextUtf8(L"Health degraded", CurrentLocaleCode());
  } else {
    detail += std::string(" | ") + lan::i18n::TranslateNativeTextUtf8(L"Service stopped", CurrentLocaleCode());
  }
  NSString* detailText = ToNSString(detail);
  NSString* badge = LocalizedWide(tick.tracker.trayIconViewModel.statusBadge);
  NSImage* statusImage = LoadMenuBarImage(MenuBarIconName(attention, viewers, sharing));
  NSStatusBarButton* button = StatusItemButton(_statusItem);

  _statusLineItem.title = status;
  _detailLineItem.title = detailText;
  if (button != nil) {
    [button setImage:statusImage];
    [button setTitle:(statusImage ? @"" : (badge.length > 0 ? badge : @"LAN Share"))];
    [button setToolTip:(detailText.length > 0 ? detailText : LocalizedWide(L"ViewMesh Host"))];
  }
  _openDashboardItem.enabled = stableRunning;
  _refreshDashboardItem.enabled = canRefreshDashboard;
  _refreshDashboardItem.title = canRefreshDashboard ? LocalizedWide(L"Refresh Dashboard") : LocalizedWide(L"Refresh Dashboard (service stopped)");
  _openViewerItem.enabled = canOpenViewer;
  _startServerItem.enabled = canStartServer;
  _startServerItem.title = canStartServer ? LocalizedWide(L"Start Sharing Service") : LocalizedWide(L"Start Sharing Service (already running)");
  _stopServerItem.enabled = canStopServer;
  _stopServerItem.title = canStopServer ? LocalizedWide(L"Stop Sharing Service") : LocalizedWide(L"Stop Sharing Service (not running)");
  _openViewerItem.title = ViewerTitle(viewers);
  _openDiagnosticsItem.enabled = canOpenDiagnostics;
  _exportDiagnosticsItem.enabled = canExportDiagnostics;
}

- (void)refresh:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->loop) return;
  _state->lastTick = _state->loop->Tick();
  _state->hasLastTick = true;
  [self applyCurrentTick];
}

- (void)openDashboard:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  if (!_state->controller->OpenDashboard(err)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  }
  [self refresh:nil];
}

- (void)refreshDashboard:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  lan::host_shell::NativeShellLiveSnapshot live;
  if (!_state->controller->RefreshDashboard(err, &live)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  } else {
    [self notifyTitleText:LocalizedWide(L"Dashboard refreshed")
                 bodyText:LocalizedWide(L"The dashboard live probe completed successfully.")];
  }
  [self refresh:nil];
}

- (void)openViewer:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  if (!_state->controller->OpenViewer(err)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  }
  [self refresh:nil];
}

- (void)startServer:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  lan::host_shell::NativeShellLiveSnapshot live;
  if (!_state->controller->StartServer(err, &live)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  } else {
    [self notifyTitleText:LocalizedWide(L"Sharing service started")
                 bodyText:LocalizedWide(L"The native sharing service is live and healthy.")];
  }
  [self refresh:nil];
}

- (void)stopServer:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  lan::host_shell::NativeShellLiveSnapshot live;
  if (!_state->controller->StopServer(err, &live)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  } else {
    [self notifyTitleText:LocalizedWide(L"Sharing service stopped")
                 bodyText:LocalizedWide(L"The native sharing service has stopped.")];
  }
  [self refresh:nil];
}

- (void)openDiagnostics:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller) return;
  std::string err;
  if (!_state->controller->OpenDiagnosticsFolder(err)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  }
  [self refresh:nil];
}

- (void)exportDiagnostics:(id)sender {
  (void)sender;
  if (_state == nullptr || !_state->controller || !_state->loop) return;
  const auto tick = _state->hasLastTick ? _state->lastTick : _state->loop->Tick();
  std::filesystem::path exported;
  std::string err;
  if (!_state->controller->ExportDiagnostics(tick.tracker.statusViewModel.statusText,
                                             tick.tracker.statusViewModel.detailText,
                                             tick.tracker.trayIconViewModel.statusBadge,
                                             err,
                                             &exported)) {
    if (err.empty()) err = lan::i18n::TranslateNativeTextUtf8(L"The requested action failed.", CurrentLocaleCode());
    [self notifyTitleText:LocalizedWide(L"Action failed") bodyText:ToNSString(err)];
  } else {
    [self notifyTitleText:LocalizedWide(L"Diagnostics exported") bodyText:ToNSString(exported.string())];
  }
  [self refresh:nil];
}

- (void)quit:(id)sender {
  (void)sender;
  [NSApp terminate:nil];
}

@end

int main(int argc, char** argv) {
  @autoreleasepool {
    lan::host_shell::NativeShellActionConfig actionConfig;
    lan::host_shell::NativeShellEndpointConfig endpoint;
    const auto executableDir = lan::platform::ExecutableDir(argv[0]);
    bool diagnosticsDirExplicit = false;
    bool serverExecutableExplicit = false;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--host" && i + 1 < argc) endpoint.host = argv[++i];
      else if (arg == "--port" && i + 1 < argc) endpoint.port = std::atoi(argv[++i]);
      else if (arg == "--room" && i + 1 < argc) {
        const char* value = argv[++i];
        actionConfig.room.assign(value, value + std::strlen(value));
      } else if (arg == "--token" && i + 1 < argc) {
        const char* value = argv[++i];
        actionConfig.token.assign(value, value + std::strlen(value));
      } else if (arg == "--diagnostics-dir" && i + 1 < argc) {
        actionConfig.diagnosticsDir = argv[++i];
        diagnosticsDirExplicit = true;
      } else if (arg == "--server-executable" && i + 1 < argc) {
        actionConfig.serverExecutable = argv[++i];
        serverExecutableExplicit = true;
      } else if (arg == "--server-arg" && i + 1 < argc) {
        actionConfig.serverArguments.emplace_back(argv[++i]);
      }
    }

    if (!diagnosticsDirExplicit) {
      actionConfig.diagnosticsDir = ResolveMacSupportDir() / "out" / "diagnostics";
    }
    if (!serverExecutableExplicit) {
      const auto defaultServerExecutable = ResolveMacDefaultServerExecutable(executableDir);
      if (!defaultServerExecutable.empty()) {
        actionConfig.serverExecutable = defaultServerExecutable;
      }
    }

    actionConfig.host = endpoint.host;
    actionConfig.port = endpoint.port;
    actionConfig.localeCode = CurrentLocaleCode();

    auto controller = std::make_unique<lan::host_shell::NativeShellActionController>(std::move(actionConfig));
    auto loop = std::make_unique<lan::host_shell::NativeShellRuntimeLoop>(lan::host_shell::MakeNativeShellPollFunction(endpoint),
                                                                          controller->Platform());

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    __unused LanMenuBarController* shell = [[LanMenuBarController alloc] init];
    auto* state = new LanMenuBarCppState(std::move(controller), std::move(loop));
    [shell bootstrapWithState:state];
    [NSApp run];
  }
  return 0;
}
