#include "host_shell/native_shell_action_controller.h"
#include "host_shell/native_shell_runtime_loop.h"

#include <dlfcn.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

using GBoolean = int;
using GUInt = unsigned int;
using GULong = unsigned long;
using GPointer = void*;
using GCallback = void (*)();
using GClosureNotify = void (*)(void*, void*);
using GSourceFunc = int (*)(void*);

struct GtkWidget;
struct GtkMenuItem;
struct AppIndicator;

constexpr int kIndicatorCategoryApplicationStatus = 0;
constexpr int kIndicatorStatusActive = 1;
constexpr int kGConnectAfter = 0;

std::string Narrow(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
}

std::string ViewerLabel(std::size_t viewers) {
  if (viewers == 0) return "Open Viewer URL";
  if (viewers == 1) return "Open Viewer URL (1 viewer)";
  return "Open Viewer URL (" + std::to_string(viewers) + " viewers)";
}

struct GtkApi {
  void* gtkHandle = nullptr;
  void* appIndicatorHandle = nullptr;

  int (*gtk_init_check)(int*, char***) = nullptr;
  void (*gtk_main)() = nullptr;
  void (*gtk_main_quit)() = nullptr;
  GtkWidget* (*gtk_menu_new)() = nullptr;
  GtkWidget* (*gtk_menu_item_new_with_label)(const char*) = nullptr;
  GtkWidget* (*gtk_separator_menu_item_new)() = nullptr;
  void (*gtk_menu_shell_append)(void*, GtkWidget*) = nullptr;
  void (*gtk_widget_show_all)(GtkWidget*) = nullptr;
  void (*gtk_widget_set_sensitive)(GtkWidget*, GBoolean) = nullptr;
  void (*gtk_menu_item_set_label)(GtkMenuItem*, const char*) = nullptr;
  GULong (*g_signal_connect_data)(GPointer, const char*, GCallback, GPointer, GClosureNotify, int) = nullptr;
  GUInt (*g_timeout_add)(GUInt, GSourceFunc, GPointer) = nullptr;

  AppIndicator* (*app_indicator_new)(const char*, const char*, int) = nullptr;
  void (*app_indicator_set_status)(AppIndicator*, int) = nullptr;
  void (*app_indicator_set_menu)(AppIndicator*, GtkWidget*) = nullptr;
  void (*app_indicator_set_label)(AppIndicator*, const char*, const char*) = nullptr;

  ~GtkApi() {
    if (appIndicatorHandle) dlclose(appIndicatorHandle);
    if (gtkHandle) dlclose(gtkHandle);
  }
};

void* LoadSymbol(void* handle, const char* name) {
  return handle ? dlsym(handle, name) : nullptr;
}

bool LoadGtkApi(GtkApi& api, std::string& err) {
  api.gtkHandle = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (!api.gtkHandle) {
    err = "GTK3/AppIndicator runtime is not installed on this system.";
    return false;
  }

  const char* indicatorLibs[] = {
      "libayatana-appindicator3.so.1",
      "libappindicator3.so.1",
  };
  for (const char* candidate : indicatorLibs) {
    api.appIndicatorHandle = dlopen(candidate, RTLD_LAZY | RTLD_LOCAL);
    if (api.appIndicatorHandle) break;
  }
  if (!api.appIndicatorHandle) {
    err = "GTK3/AppIndicator runtime is not installed on this system.";
    return false;
  }

  api.gtk_init_check = reinterpret_cast<int (*)(int*, char***)>(LoadSymbol(api.gtkHandle, "gtk_init_check"));
  api.gtk_main = reinterpret_cast<void (*)()>(LoadSymbol(api.gtkHandle, "gtk_main"));
  api.gtk_main_quit = reinterpret_cast<void (*)()>(LoadSymbol(api.gtkHandle, "gtk_main_quit"));
  api.gtk_menu_new = reinterpret_cast<GtkWidget* (*)()>(LoadSymbol(api.gtkHandle, "gtk_menu_new"));
  api.gtk_menu_item_new_with_label = reinterpret_cast<GtkWidget* (*)(const char*)>(LoadSymbol(api.gtkHandle, "gtk_menu_item_new_with_label"));
  api.gtk_separator_menu_item_new = reinterpret_cast<GtkWidget* (*)()>(LoadSymbol(api.gtkHandle, "gtk_separator_menu_item_new"));
  api.gtk_menu_shell_append = reinterpret_cast<void (*)(void*, GtkWidget*)>(LoadSymbol(api.gtkHandle, "gtk_menu_shell_append"));
  api.gtk_widget_show_all = reinterpret_cast<void (*)(GtkWidget*)>(LoadSymbol(api.gtkHandle, "gtk_widget_show_all"));
  api.gtk_widget_set_sensitive = reinterpret_cast<void (*)(GtkWidget*, GBoolean)>(LoadSymbol(api.gtkHandle, "gtk_widget_set_sensitive"));
  api.gtk_menu_item_set_label = reinterpret_cast<void (*)(GtkMenuItem*, const char*)>(LoadSymbol(api.gtkHandle, "gtk_menu_item_set_label"));
  api.g_signal_connect_data = reinterpret_cast<GULong (*)(GPointer, const char*, GCallback, GPointer, GClosureNotify, int)>(LoadSymbol(api.gtkHandle, "g_signal_connect_data"));
  api.g_timeout_add = reinterpret_cast<GUInt (*)(GUInt, GSourceFunc, GPointer)>(LoadSymbol(api.gtkHandle, "g_timeout_add"));

  api.app_indicator_new = reinterpret_cast<AppIndicator* (*)(const char*, const char*, int)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_new"));
  api.app_indicator_set_status = reinterpret_cast<void (*)(AppIndicator*, int)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_status"));
  api.app_indicator_set_menu = reinterpret_cast<void (*)(AppIndicator*, GtkWidget*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_menu"));
  api.app_indicator_set_label = reinterpret_cast<void (*)(AppIndicator*, const char*, const char*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_label"));

  if (!api.gtk_init_check || !api.gtk_main || !api.gtk_main_quit || !api.gtk_menu_new ||
      !api.gtk_menu_item_new_with_label || !api.gtk_separator_menu_item_new || !api.gtk_menu_shell_append ||
      !api.gtk_widget_show_all || !api.gtk_widget_set_sensitive || !api.gtk_menu_item_set_label ||
      !api.g_signal_connect_data || !api.g_timeout_add || !api.app_indicator_new ||
      !api.app_indicator_set_status || !api.app_indicator_set_menu || !api.app_indicator_set_label) {
    err = "GTK3/AppIndicator runtime is present but missing expected symbols.";
    return false;
  }

  err.clear();
  return true;
}

class LinuxTrayApp {
public:
  LinuxTrayApp(GtkApi& api,
               lan::host_shell::NativeShellActionController controller,
               lan::host_shell::NativeShellRuntimeLoop loop)
      : api_(api), controller_(std::move(controller)), loop_(std::move(loop)) {}

  bool Initialize(std::string& err) {
    int argc = 0;
    char** argv = nullptr;
    if (!api_.gtk_init_check(&argc, &argv)) {
      err = "GTK failed to initialize.";
      return false;
    }

    menu_ = api_.gtk_menu_new();
    if (!menu_) {
      err = "Failed to create GTK menu.";
      return false;
    }

    statusItem_ = api_.gtk_menu_item_new_with_label("Status: starting");
    detailItem_ = api_.gtk_menu_item_new_with_label("Waiting for first refresh...");
    auto* separator1 = api_.gtk_separator_menu_item_new();
    openDashboardItem_ = api_.gtk_menu_item_new_with_label("Open Dashboard");
    refreshDashboardItem_ = api_.gtk_menu_item_new_with_label("Refresh Dashboard");
    openViewerItem_ = api_.gtk_menu_item_new_with_label("Open Viewer URL");
    startServerItem_ = api_.gtk_menu_item_new_with_label("Start Sharing Service");
    stopServerItem_ = api_.gtk_menu_item_new_with_label("Stop Sharing Service");
    openDiagnosticsItem_ = api_.gtk_menu_item_new_with_label("Open Diagnostics Folder");
    exportDiagnosticsItem_ = api_.gtk_menu_item_new_with_label("Export Diagnostics Snapshot");
    auto* separator2 = api_.gtk_separator_menu_item_new();
    quitItem_ = api_.gtk_menu_item_new_with_label("Quit");

    api_.gtk_widget_set_sensitive(statusItem_, 0);
    api_.gtk_widget_set_sensitive(detailItem_, 0);

    Append(statusItem_);
    Append(detailItem_);
    Append(separator1);
    Append(openDashboardItem_);
    Append(refreshDashboardItem_);
    Append(openViewerItem_);
    Append(startServerItem_);
    Append(stopServerItem_);
    Append(openDiagnosticsItem_);
    Append(exportDiagnosticsItem_);
    Append(separator2);
    Append(quitItem_);

    Connect(openDashboardItem_, &LinuxTrayApp::OnOpenDashboard);
    Connect(refreshDashboardItem_, &LinuxTrayApp::OnRefreshDashboard);
    Connect(openViewerItem_, &LinuxTrayApp::OnOpenViewer);
    Connect(startServerItem_, &LinuxTrayApp::OnStartServer);
    Connect(stopServerItem_, &LinuxTrayApp::OnStopServer);
    Connect(openDiagnosticsItem_, &LinuxTrayApp::OnOpenDiagnostics);
    Connect(exportDiagnosticsItem_, &LinuxTrayApp::OnExportDiagnostics);
    Connect(quitItem_, &LinuxTrayApp::OnQuit);

    indicator_ = api_.app_indicator_new("lan-screenshare-native-shell", "network-workgroup", kIndicatorCategoryApplicationStatus);
    if (!indicator_) {
      err = "Failed to create AppIndicator instance.";
      return false;
    }
    api_.app_indicator_set_status(indicator_, kIndicatorStatusActive);
    api_.app_indicator_set_menu(indicator_, menu_);
    api_.gtk_widget_show_all(menu_);

    Tick();
    api_.g_timeout_add(2000, &LinuxTrayApp::OnPoll, this);
    return true;
  }

  int Run() {
    api_.gtk_main();
    return 0;
  }

private:
  void Append(GtkWidget* item) {
    api_.gtk_menu_shell_append(menu_, item);
  }

  void Connect(GtkWidget* item, void (*fn)(GtkWidget*, void*)) {
    api_.g_signal_connect_data(item, "activate", reinterpret_cast<GCallback>(fn), this, nullptr, kGConnectAfter);
  }

  void Notify(std::string_view title, std::string_view body) {
    std::string err;
    if (!controller_.Platform().ShowNotification(title, body, err) && !err.empty()) {
      std::cerr << "notification error: " << err << "\n";
    }
  }

  void ApplyTick(const lan::host_shell::NativeShellRuntimeLoopResult& tick) {
    const auto status = Narrow(tick.tracker.statusViewModel.statusText);
    const auto detail = Narrow(tick.tracker.statusViewModel.detailText);
    const auto badge = Narrow(tick.tracker.trayIconViewModel.statusBadge);
    const bool stableRunning = tick.tracker.memory.stableServerRunning;
    const bool stableHealthy = tick.tracker.memory.stableHealthReady;
    const bool attention = tick.tracker.chromeInput.attentionNeeded;
    const auto viewers = tick.tracker.memory.stableViewerCount;
    const bool canRefreshDashboard = stableRunning;
    const bool canOpenViewer = stableRunning && stableHealthy && tick.tracker.trayMenuViewModel.copyViewerUrlEnabled;
    const bool canStartServer = !stableRunning;
    const bool canStopServer = stableRunning || controller_.IsServerRunning();
    const bool canOpenDiagnostics = stableRunning || attention;
    const bool canExportDiagnostics = canOpenDiagnostics || viewers > 0;

    std::string detailLine = detail;
    if (stableRunning) {
      detailLine += stableHealthy ? " | Healthy" : " | Health degraded";
    } else {
      detailLine += " | Service stopped";
    }

    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(statusItem_), status.c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(detailItem_), detailLine.c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(refreshDashboardItem_),
                                 canRefreshDashboard ? "Refresh Dashboard" : "Refresh Dashboard (service stopped)");
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(openViewerItem_), ViewerLabel(viewers).c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(startServerItem_),
                                 canStartServer ? "Start Sharing Service" : "Start Sharing Service (already running)");
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(stopServerItem_),
                                 canStopServer ? "Stop Sharing Service" : "Stop Sharing Service (not running)");
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(openDiagnosticsItem_),
                                 canOpenDiagnostics ? "Open Diagnostics Folder" : "Open Diagnostics Folder (service stopped)");
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(exportDiagnosticsItem_),
                                 canExportDiagnostics ? "Export Diagnostics Snapshot" : "Export Diagnostics Snapshot (no live data yet)");

    api_.gtk_widget_set_sensitive(openDashboardItem_, stableRunning ? 1 : 0);
    api_.gtk_widget_set_sensitive(refreshDashboardItem_, canRefreshDashboard ? 1 : 0);
    api_.gtk_widget_set_sensitive(openViewerItem_, canOpenViewer ? 1 : 0);
    api_.gtk_widget_set_sensitive(startServerItem_, canStartServer ? 1 : 0);
    api_.gtk_widget_set_sensitive(stopServerItem_, canStopServer ? 1 : 0);
    api_.gtk_widget_set_sensitive(openDiagnosticsItem_, canOpenDiagnostics ? 1 : 0);
    api_.gtk_widget_set_sensitive(exportDiagnosticsItem_, canExportDiagnostics ? 1 : 0);
    api_.app_indicator_set_label(indicator_, badge.c_str(), badge.c_str());
  }

  void Tick() {
    lastTick_ = loop_.Tick();
    hasLastTick_ = true;
    ApplyTick(lastTick_);
  }

  template <typename Fn>
  void RunAction(Fn&& action, std::string_view successTitle = {}, std::string successBody = {}, bool notifyOnSuccess = false) {
    std::string err;
    if (!action(err)) {
      if (err.empty()) err = "The requested action failed.";
      Notify("Action failed", err);
    } else if (notifyOnSuccess && !successTitle.empty()) {
      Notify(successTitle, successBody.empty() ? std::string{"The action completed successfully."} : successBody);
    }
    Tick();
  }

  static int OnPoll(void* userdata) {
    static_cast<LinuxTrayApp*>(userdata)->Tick();
    return 1;
  }

  static void OnOpenDashboard(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    self->RunAction([&](std::string& err) { return self->controller_.OpenDashboard(err); });
  }

  static void OnRefreshDashboard(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    self->RunAction([&](std::string& err) {
      lan::host_shell::NativeShellLiveSnapshot live;
      return self->controller_.RefreshDashboard(err, &live);
    }, "Dashboard refreshed", "The dashboard live probe completed successfully.", true);
  }

  static void OnOpenViewer(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    self->RunAction([&](std::string& err) { return self->controller_.OpenViewer(err); });
  }

  static void OnStartServer(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    self->RunAction([&](std::string& err) {
      lan::host_shell::NativeShellLiveSnapshot live;
      return self->controller_.StartServer(err, &live);
    }, "Sharing service started", "The native sharing service is live and healthy.", true);
  }

  static void OnStopServer(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    self->RunAction([&](std::string& err) {
      lan::host_shell::NativeShellLiveSnapshot live;
      return self->controller_.StopServer(err, &live);
    }, "Sharing service stopped", "The native sharing service has stopped.", true);
  }

  static void OnOpenDiagnostics(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    self->RunAction([&](std::string& err) { return self->controller_.OpenDiagnosticsFolder(err); });
  }

  static void OnExportDiagnostics(GtkWidget*, void* userdata) {
    auto* self = static_cast<LinuxTrayApp*>(userdata);
    const auto tick = self->hasLastTick_ ? self->lastTick_ : self->loop_.Tick();
    std::filesystem::path exported;
    std::string err;
    if (!self->controller_.ExportDiagnostics(tick.tracker.statusViewModel.statusText,
                                             tick.tracker.statusViewModel.detailText,
                                             tick.tracker.trayIconViewModel.statusBadge,
                                             err,
                                             &exported)) {
      if (err.empty()) err = "Failed to export diagnostics snapshot.";
      self->Notify("Action failed", err);
    } else {
      self->Notify("Diagnostics exported", exported.string());
    }
    self->Tick();
  }

  static void OnQuit(GtkWidget*, void* userdata) {
    static_cast<LinuxTrayApp*>(userdata)->api_.gtk_main_quit();
  }

  GtkApi& api_;
  lan::host_shell::NativeShellActionController controller_;
  lan::host_shell::NativeShellRuntimeLoop loop_;
  lan::host_shell::NativeShellRuntimeLoopResult lastTick_{};
  bool hasLastTick_ = false;
  AppIndicator* indicator_ = nullptr;
  GtkWidget* menu_ = nullptr;
  GtkWidget* statusItem_ = nullptr;
  GtkWidget* detailItem_ = nullptr;
  GtkWidget* openDashboardItem_ = nullptr;
  GtkWidget* refreshDashboardItem_ = nullptr;
  GtkWidget* openViewerItem_ = nullptr;
  GtkWidget* startServerItem_ = nullptr;
  GtkWidget* stopServerItem_ = nullptr;
  GtkWidget* openDiagnosticsItem_ = nullptr;
  GtkWidget* exportDiagnosticsItem_ = nullptr;
  GtkWidget* quitItem_ = nullptr;
};

} // namespace

int main(int argc, char** argv) {
  lan::host_shell::NativeShellActionConfig actionConfig;
  lan::host_shell::NativeShellEndpointConfig endpoint;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc) endpoint.host = argv[++i];
    else if (arg == "--port" && i + 1 < argc) endpoint.port = std::atoi(argv[++i]);
    else if (arg == "--room" && i + 1 < argc) { const char* value = argv[++i]; actionConfig.room.assign(value, value + std::strlen(value)); }
    else if (arg == "--token" && i + 1 < argc) { const char* value = argv[++i]; actionConfig.token.assign(value, value + std::strlen(value)); }
    else if (arg == "--diagnostics-dir" && i + 1 < argc) actionConfig.diagnosticsDir = argv[++i];
    else if (arg == "--server-executable" && i + 1 < argc) actionConfig.serverExecutable = argv[++i];
    else if (arg == "--server-arg" && i + 1 < argc) actionConfig.serverArguments.emplace_back(argv[++i]);
  }
  actionConfig.host = endpoint.host;
  actionConfig.port = endpoint.port;

  GtkApi api;
  std::string err;
  if (!LoadGtkApi(api, err)) {
    std::cerr << err << "\n";
    return 1;
  }

  auto controller = lan::host_shell::NativeShellActionController(std::move(actionConfig));
  auto loop = lan::host_shell::NativeShellRuntimeLoop(lan::host_shell::MakeNativeShellPollFunction(endpoint),
                                                      controller.Platform());

  LinuxTrayApp app(api, std::move(controller), std::move(loop));
  if (!app.Initialize(err)) {
    std::cerr << err << "\n";
    return 1;
  }
  return app.Run();
}
