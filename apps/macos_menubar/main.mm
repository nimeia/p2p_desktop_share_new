#import <Cocoa/Cocoa.h>

#include "host_shell/native_shell_action_controller.h"
#include "host_shell/native_shell_runtime_loop.h"

#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

namespace {

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

std::string Narrow(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
}

NSString* ViewerTitle(std::size_t viewers) {
  if (viewers == 0) return @"Open Viewer URL";
  if (viewers == 1) return @"Open Viewer URL (1 viewer)";
  return [NSString stringWithFormat:@"Open Viewer URL (%zu viewers)", viewers];
}

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
  std::unique_ptr<lan::host_shell::NativeShellActionController> _controller;
  std::unique_ptr<lan::host_shell::NativeShellRuntimeLoop> _loop;
  lan::host_shell::NativeShellRuntimeLoopResult _lastTick;
  BOOL _hasLastTick;
}
- (instancetype)initWithController:(std::unique_ptr<lan::host_shell::NativeShellActionController>)controller
                              loop:(std::unique_ptr<lan::host_shell::NativeShellRuntimeLoop>)loop;
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

- (instancetype)initWithController:(std::unique_ptr<lan::host_shell::NativeShellActionController>)controller
                              loop:(std::unique_ptr<lan::host_shell::NativeShellRuntimeLoop>)loop {
  self = [super init];
  if (!self) return nil;

  _controller = std::move(controller);
  _loop = std::move(loop);
  _hasLastTick = NO;

  _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
  _statusItem.button.title = @"LAN Share";
  _statusItem.button.toolTip = @"LAN Screen Share Host";

  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"LAN Screen Share Host"];
  _statusLineItem = [[NSMenuItem alloc] initWithTitle:@"Status: starting" action:nil keyEquivalent:@""];
  _detailLineItem = [[NSMenuItem alloc] initWithTitle:@"Waiting for first refresh..." action:nil keyEquivalent:@""];
  _statusLineItem.enabled = NO;
  _detailLineItem.enabled = NO;
  [menu addItem:_statusLineItem];
  [menu addItem:_detailLineItem];
  [menu addItem:[NSMenuItem separatorItem]];

  _openDashboardItem = [[NSMenuItem alloc] initWithTitle:@"Open Dashboard" action:@selector(openDashboard:) keyEquivalent:@""];
  _openDashboardItem.target = self;
  [menu addItem:_openDashboardItem];

  _refreshDashboardItem = [[NSMenuItem alloc] initWithTitle:@"Refresh Dashboard" action:@selector(refreshDashboard:) keyEquivalent:@""];
  _refreshDashboardItem.target = self;
  [menu addItem:_refreshDashboardItem];

  _openViewerItem = [[NSMenuItem alloc] initWithTitle:@"Open Viewer URL" action:@selector(openViewer:) keyEquivalent:@""];
  _openViewerItem.target = self;
  [menu addItem:_openViewerItem];

  _startServerItem = [[NSMenuItem alloc] initWithTitle:@"Start Sharing Service" action:@selector(startServer:) keyEquivalent:@""];
  _startServerItem.target = self;
  [menu addItem:_startServerItem];

  _stopServerItem = [[NSMenuItem alloc] initWithTitle:@"Stop Sharing Service" action:@selector(stopServer:) keyEquivalent:@""];
  _stopServerItem.target = self;
  [menu addItem:_stopServerItem];

  _openDiagnosticsItem = [[NSMenuItem alloc] initWithTitle:@"Open Diagnostics Folder" action:@selector(openDiagnostics:) keyEquivalent:@""];
  _openDiagnosticsItem.target = self;
  [menu addItem:_openDiagnosticsItem];

  _exportDiagnosticsItem = [[NSMenuItem alloc] initWithTitle:@"Export Diagnostics Snapshot" action:@selector(exportDiagnostics:) keyEquivalent:@""];
  _exportDiagnosticsItem.target = self;
  [menu addItem:_exportDiagnosticsItem];

  [menu addItem:[NSMenuItem separatorItem]];
  NSMenuItem* quitItem = [[NSMenuItem alloc] initWithTitle:@"Quit" action:@selector(quit:) keyEquivalent:@"q"];
  quitItem.target = self;
  [menu addItem:quitItem];

  _statusItem.menu = menu;
  [self refresh:nil];
  _timer = [NSTimer scheduledTimerWithTimeInterval:2.0 target:self selector:@selector(refresh:) userInfo:nil repeats:YES];
  return self;
}

- (void)notifyTitle:(std::string_view)title body:(std::string_view)body {
  std::string err;
  if (!_controller->Platform().ShowNotification(title, body, err) && !err.empty()) {
    NSLog(@"Notification error: %s", err.c_str());
  }
}

- (void)applyTick:(const lan::host_shell::NativeShellRuntimeLoopResult&)tick {
  const auto status = ToNSString(Narrow(tick.tracker.statusViewModel.statusText));
  const bool stableRunning = tick.tracker.memory.stableServerRunning;
  const bool stableHealthy = tick.tracker.memory.stableHealthReady;
  const bool attention = tick.tracker.chromeInput.attentionNeeded;
  const auto viewers = tick.tracker.memory.stableViewerCount;
  const bool canRefreshDashboard = stableRunning;
  const bool canOpenViewer = stableRunning && stableHealthy && tick.tracker.trayMenuViewModel.copyViewerUrlEnabled;
  const bool canStartServer = !stableRunning;
  const bool canStopServer = stableRunning || _controller->IsServerRunning();
  const bool canOpenDiagnostics = stableRunning || attention;
  const bool canExportDiagnostics = canOpenDiagnostics || viewers > 0;

  std::string detail = Narrow(tick.tracker.statusViewModel.detailText);
  if (stableRunning) {
    detail += stableHealthy ? " | Healthy" : " | Health degraded";
  } else {
    detail += " | Service stopped";
  }
  NSString* detailText = ToNSString(detail);
  NSString* badge = ToNSString(Narrow(tick.tracker.trayIconViewModel.statusBadge));

  _statusLineItem.title = status;
  _detailLineItem.title = detailText;
  _statusItem.button.title = badge.length > 0 ? badge : @"LAN Share";
  _statusItem.button.toolTip = detailText.length > 0 ? detailText : @"LAN Screen Share Host";
  _openDashboardItem.enabled = stableRunning;
  _refreshDashboardItem.enabled = canRefreshDashboard;
  _refreshDashboardItem.title = canRefreshDashboard ? @"Refresh Dashboard" : @"Refresh Dashboard (service stopped)";
  _openViewerItem.enabled = canOpenViewer;
  _startServerItem.enabled = canStartServer;
  _startServerItem.title = canStartServer ? @"Start Sharing Service" : @"Start Sharing Service (already running)";
  _stopServerItem.enabled = canStopServer;
  _stopServerItem.title = canStopServer ? @"Stop Sharing Service" : @"Stop Sharing Service (not running)";
  _openViewerItem.title = ViewerTitle(viewers);
  _openDiagnosticsItem.enabled = canOpenDiagnostics;
  _exportDiagnosticsItem.enabled = canExportDiagnostics;
}

- (void)refresh:(id)sender {
  (void)sender;
  _lastTick = _loop->Tick();
  _hasLastTick = YES;
  [self applyTick:_lastTick];
}

- (void)runAction:(BOOL (^)(std::string& err))action
      successTitle:(std::string_view)successTitle
       successBody:(std::string)successBody
   notifyOnSuccess:(BOOL)notifyOnSuccess {
  std::string err;
  if (!action(err)) {
    if (err.empty()) err = "The requested action failed.";
    [self notifyTitle:"Action failed" body:err];
  } else if (notifyOnSuccess && !successTitle.empty()) {
    [self notifyTitle:successTitle body:successBody.empty() ? std::string{"The action completed successfully."} : successBody];
  }
  [self refresh:nil];
}

- (void)openDashboard:(id)sender {
  (void)sender;
  [self runAction:^BOOL(std::string& err) {
    return _controller->OpenDashboard(err);
  } successTitle:std::string_view{} successBody:std::string{} notifyOnSuccess:NO];
}

- (void)refreshDashboard:(id)sender {
  (void)sender;
  [self runAction:^BOOL(std::string& err) {
    lan::host_shell::NativeShellLiveSnapshot live;
    return _controller->RefreshDashboard(err, &live);
  } successTitle:"Dashboard refreshed" successBody:"The dashboard live probe completed successfully." notifyOnSuccess:YES];
}

- (void)openViewer:(id)sender {
  (void)sender;
  [self runAction:^BOOL(std::string& err) {
    return _controller->OpenViewer(err);
  } successTitle:std::string_view{} successBody:std::string{} notifyOnSuccess:NO];
}

- (void)startServer:(id)sender {
  (void)sender;
  [self runAction:^BOOL(std::string& err) {
    lan::host_shell::NativeShellLiveSnapshot live;
    return _controller->StartServer(err, &live);
  } successTitle:"Sharing service started" successBody:"The native sharing service is live and healthy." notifyOnSuccess:YES];
}

- (void)stopServer:(id)sender {
  (void)sender;
  [self runAction:^BOOL(std::string& err) {
    lan::host_shell::NativeShellLiveSnapshot live;
    return _controller->StopServer(err, &live);
  } successTitle:"Sharing service stopped" successBody:"The native sharing service has stopped." notifyOnSuccess:YES];
}

- (void)openDiagnostics:(id)sender {
  (void)sender;
  [self runAction:^BOOL(std::string& err) {
    return _controller->OpenDiagnosticsFolder(err);
  } successTitle:std::string_view{} successBody:std::string{} notifyOnSuccess:NO];
}

- (void)exportDiagnostics:(id)sender {
  (void)sender;
  const auto tick = _hasLastTick ? _lastTick : _loop->Tick();
  std::filesystem::path exported;
  std::string successBody;
  [self runAction:^BOOL(std::string& err) {
    const bool ok = _controller->ExportDiagnostics(tick.tracker.statusViewModel.statusText,
                                                   tick.tracker.statusViewModel.detailText,
                                                   tick.tracker.trayIconViewModel.statusBadge,
                                                   err,
                                                   &exported);
    if (ok) successBody = exported.string();
    return ok;
  } successTitle:"Diagnostics exported" successBody:successBody notifyOnSuccess:YES];
}

- (void)quit:(id)sender {
  (void)sender;
  [NSApp terminate:nil];
}

@end

} // namespace

int main(int argc, char** argv) {
  @autoreleasepool {
    lan::host_shell::NativeShellActionConfig actionConfig;
    lan::host_shell::NativeShellEndpointConfig endpoint;

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
      } else if (arg == "--server-executable" && i + 1 < argc) {
        actionConfig.serverExecutable = argv[++i];
      } else if (arg == "--server-arg" && i + 1 < argc) {
        actionConfig.serverArguments.emplace_back(argv[++i]);
      }
    }
    actionConfig.host = endpoint.host;
    actionConfig.port = endpoint.port;

    auto controller = std::make_unique<lan::host_shell::NativeShellActionController>(std::move(actionConfig));
    auto loop = std::make_unique<lan::host_shell::NativeShellRuntimeLoop>(lan::host_shell::MakeNativeShellPollFunction(endpoint),
                                                                          controller->Platform());

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    __unused LanMenuBarController* shell = [[LanMenuBarController alloc] initWithController:std::move(controller)
                                                                                       loop:std::move(loop)];
    [NSApp run];
  }
  return 0;
}
