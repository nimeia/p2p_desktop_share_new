#include "host_shell/native_shell_action_controller.h"
#include "host_shell/native_shell_runtime_loop.h"
#include "core/i18n/localization.h"
#include "platform/abstraction/runtime_paths.h"

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
constexpr const char* kFallbackIndicatorIcon = "lanscreenshare";

enum class LinuxTrayIconState {
  Base,
  Alert,
  Connected,
  Sharing,
};

std::string Narrow(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
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

std::string TranslateWide(std::wstring_view source) {
  return lan::i18n::TranslateNativeTextUtf8(source, CurrentLocaleCode());
}

std::string TranslateAscii(std::string_view source) {
  for (unsigned char ch : source) {
    if (ch >= 0x80) {
      return std::string(source);
    }
  }
  return lan::i18n::TranslateNativeTextUtf8(WideAscii(source), CurrentLocaleCode());
}

std::string ViewerLabel(std::size_t viewers) {
  if (viewers == 0) return TranslateWide(L"Open Viewer URL");
  if (viewers == 1) return TranslateWide(L"Open Viewer URL (1 viewer)");
  return TranslateWide(L"Open Viewer URL (" + std::to_wstring(viewers) + L" viewers)");
}

std::filesystem::path ResolveLinuxStateDir() {
  if (const char* stateHome = std::getenv("XDG_STATE_HOME")) {
    if (*stateHome) return std::filesystem::path(stateHome) / "viewmesh";
  }
  if (const char* home = std::getenv("HOME")) {
    if (*home) return std::filesystem::path(home) / ".local" / "state" / "viewmesh";
  }
  return std::filesystem::current_path() / ".viewmesh";
}

std::filesystem::path ResolveLinuxDefaultServerExecutable(const std::filesystem::path& executableDir) {
  const std::filesystem::path candidates[] = {
      executableDir / "ViewMeshServer",
      executableDir / "lan_screenshare_server",
      executableDir.parent_path() / "server" / "ViewMeshServer",
      executableDir.parent_path() / "server" / "lan_screenshare_server",
      executableDir.parent_path() / "runtime" / "ViewMeshServer",
      executableDir.parent_path() / "runtime" / "lan_screenshare_server",
      executableDir.parent_path().parent_path() / "runtime" / "ViewMeshServer",
      executableDir.parent_path().parent_path() / "runtime" / "lan_screenshare_server",
  };

  for (const auto& candidate : candidates) {
    if (!candidate.empty() && std::filesystem::exists(candidate)) return candidate;
  }
  return {};
}

std::vector<std::filesystem::path> BuildSearchRoots(const std::filesystem::path& start) {
  std::vector<std::filesystem::path> roots;
  for (std::filesystem::path current = start; !current.empty();) {
    roots.push_back(current);
    const auto parent = current.parent_path();
    if (parent == current) break;
    current = parent;
  }
  return roots;
}

bool HasTrayIconSet(const std::filesystem::path& root) {
  return std::filesystem::exists(root / "tray" / "lanscreenshare-tray-24.png") &&
         std::filesystem::exists(root / "tray_states" / "color" / "lanscreenshare-tray-alert-24.png") &&
         std::filesystem::exists(root / "tray_states" / "color" / "lanscreenshare-tray-connected-24.png") &&
         std::filesystem::exists(root / "tray_states" / "color" / "lanscreenshare-tray-sharing-24.png");
}

std::filesystem::path DiscoverLinuxIconRoot(const std::filesystem::path& executableDir) {
  std::vector<std::filesystem::path> searchRoots = BuildSearchRoots(executableDir);
  try {
    const auto cwdRoots = BuildSearchRoots(std::filesystem::current_path());
    searchRoots.insert(searchRoots.end(), cwdRoots.begin(), cwdRoots.end());
  } catch (...) {
  }

  for (const auto& root : searchRoots) {
    const auto sourceRoot = root / "src" / "resources" / "icons" / "linux";
    if (HasTrayIconSet(sourceRoot)) return sourceRoot;

    const auto stagedRoot = root / "icons" / "linux";
    if (HasTrayIconSet(stagedRoot)) return stagedRoot;

    const auto installedRoot = root / "share" / "viewmesh" / "icons" / "linux";
    if (HasTrayIconSet(installedRoot)) return installedRoot;

    const auto legacyInstalledRoot = root / "share" / "lan_screenshare" / "icons" / "linux";
    if (HasTrayIconSet(legacyInstalledRoot)) return legacyInstalledRoot;
  }

  const std::filesystem::path systemRoots[] = {
      "/usr/local/share/viewmesh/icons/linux",
      "/usr/share/viewmesh/icons/linux",
      "/usr/local/share/lan_screenshare/icons/linux",
      "/usr/share/lan_screenshare/icons/linux",
  };
  for (const auto& root : systemRoots) {
    if (HasTrayIconSet(root)) return root;
  }

  return {};
}

LinuxTrayIconState SelectLinuxTrayIconState(const lan::host_shell::NativeShellRuntimeLoopResult& tick) {
  if (tick.tracker.chromeInput.attentionNeeded) return LinuxTrayIconState::Alert;
  if (tick.tracker.chromeInput.viewerCount > 0) return LinuxTrayIconState::Connected;
  if (tick.tracker.chromeInput.serverRunning && tick.tracker.chromeInput.hostStateSharing) return LinuxTrayIconState::Sharing;
  return LinuxTrayIconState::Base;
}

std::filesystem::path IconPathForState(const std::filesystem::path& iconRoot, LinuxTrayIconState state) {
  switch (state) {
    case LinuxTrayIconState::Alert:
      return iconRoot / "tray_states" / "color" / "lanscreenshare-tray-alert-24.png";
    case LinuxTrayIconState::Connected:
      return iconRoot / "tray_states" / "color" / "lanscreenshare-tray-connected-24.png";
    case LinuxTrayIconState::Sharing:
      return iconRoot / "tray_states" / "color" / "lanscreenshare-tray-sharing-24.png";
    case LinuxTrayIconState::Base:
    default:
      return iconRoot / "tray" / "lanscreenshare-tray-24.png";
  }
}

std::filesystem::path IconDirectoryForState(const std::filesystem::path& iconRoot, LinuxTrayIconState state) {
  switch (state) {
    case LinuxTrayIconState::Alert:
    case LinuxTrayIconState::Connected:
    case LinuxTrayIconState::Sharing:
      return iconRoot / "tray_states" / "color";
    case LinuxTrayIconState::Base:
    default:
      return iconRoot / "tray";
  }
}

std::string IconNameForState(LinuxTrayIconState state) {
  switch (state) {
    case LinuxTrayIconState::Alert:
      return "lanscreenshare-tray-alert-24";
    case LinuxTrayIconState::Connected:
      return "lanscreenshare-tray-connected-24";
    case LinuxTrayIconState::Sharing:
      return "lanscreenshare-tray-sharing-24";
    case LinuxTrayIconState::Base:
    default:
      return "lanscreenshare-tray-24";
  }
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
  AppIndicator* (*app_indicator_new_with_path)(const char*, const char*, int, const char*) = nullptr;
  void (*app_indicator_set_status)(AppIndicator*, int) = nullptr;
  void (*app_indicator_set_menu)(AppIndicator*, GtkWidget*) = nullptr;
  void (*app_indicator_set_label)(AppIndicator*, const char*, const char*) = nullptr;
  void (*app_indicator_set_icon)(AppIndicator*, const char*) = nullptr;
  void (*app_indicator_set_icon_full)(AppIndicator*, const char*, const char*) = nullptr;
  void (*app_indicator_set_icon_theme_path)(AppIndicator*, const char*) = nullptr;

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
  api.app_indicator_new_with_path = reinterpret_cast<AppIndicator* (*)(const char*, const char*, int, const char*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_new_with_path"));
  api.app_indicator_set_status = reinterpret_cast<void (*)(AppIndicator*, int)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_status"));
  api.app_indicator_set_menu = reinterpret_cast<void (*)(AppIndicator*, GtkWidget*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_menu"));
  api.app_indicator_set_label = reinterpret_cast<void (*)(AppIndicator*, const char*, const char*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_label"));
  api.app_indicator_set_icon = reinterpret_cast<void (*)(AppIndicator*, const char*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_icon"));
  api.app_indicator_set_icon_full = reinterpret_cast<void (*)(AppIndicator*, const char*, const char*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_icon_full"));
  api.app_indicator_set_icon_theme_path = reinterpret_cast<void (*)(AppIndicator*, const char*)>(LoadSymbol(api.appIndicatorHandle, "app_indicator_set_icon_theme_path"));

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
               lan::host_shell::NativeShellRuntimeLoop loop,
               std::filesystem::path iconRoot)
      : api_(api), controller_(std::move(controller)), loop_(std::move(loop)), iconRoot_(std::move(iconRoot)) {}

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

    statusItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Status: starting").c_str());
    detailItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Waiting for first refresh...").c_str());
    auto* separator1 = api_.gtk_separator_menu_item_new();
    openDashboardItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Open Dashboard").c_str());
    refreshDashboardItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Refresh Dashboard").c_str());
    openViewerItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Open Viewer URL").c_str());
    startServerItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Start Sharing Service").c_str());
    stopServerItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Stop Sharing Service").c_str());
    openDiagnosticsItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Open Diagnostics Folder").c_str());
    exportDiagnosticsItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Export Diagnostics Snapshot").c_str());
    auto* separator2 = api_.gtk_separator_menu_item_new();
    quitItem_ = api_.gtk_menu_item_new_with_label(TranslateWide(L"Quit").c_str());

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

    std::filesystem::path initialThemePath;
    const std::string initialIcon = InitialIndicatorIcon(initialThemePath);
    const std::string initialThemePathString = initialThemePath.string();
    if (!initialThemePathString.empty() && api_.app_indicator_new_with_path) {
      indicator_ = api_.app_indicator_new_with_path("lan-screenshare-native-shell",
                                                    initialIcon.c_str(),
                                                    kIndicatorCategoryApplicationStatus,
                                                    initialThemePathString.c_str());
    } else {
      indicator_ = api_.app_indicator_new("lan-screenshare-native-shell", initialIcon.c_str(), kIndicatorCategoryApplicationStatus);
      if (indicator_ && !initialThemePathString.empty() && api_.app_indicator_set_icon_theme_path) {
        api_.app_indicator_set_icon_theme_path(indicator_, initialThemePathString.c_str());
      }
    }
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
    const std::string translatedTitle = TranslateAscii(title);
    const std::string translatedBody = TranslateAscii(body);
    if (!controller_.Platform().ShowNotification(translatedTitle, translatedBody, err) && !err.empty()) {
      std::cerr << "notification error: " << err << "\n";
    }
  }

  bool ResolveIconSpec(LinuxTrayIconState state, std::filesystem::path& themePath, std::string& iconName) const {
    const auto path = IconPathForState(iconRoot_, state);
    if (!std::filesystem::exists(path)) return false;

    themePath = IconDirectoryForState(iconRoot_, state);
    iconName = IconNameForState(state);
    return !themePath.empty() && !iconName.empty();
  }

  std::string InitialIndicatorIcon(std::filesystem::path& themePath) const {
    std::string iconName;
    if (ResolveIconSpec(LinuxTrayIconState::Base, themePath, iconName)) {
      return iconName;
    }
    themePath.clear();
    return kFallbackIndicatorIcon;
  }

  void UpdateIndicatorIcon(const lan::host_shell::NativeShellRuntimeLoopResult& tick) {
    std::filesystem::path themePath;
    std::string iconName;
    if (!ResolveIconSpec(SelectLinuxTrayIconState(tick), themePath, iconName)) return;

    const std::string themePathString = themePath.string();
    const std::string iconKey = themePathString + "|" + iconName;
    if (iconKey == currentIconKey_) return;

    if (api_.app_indicator_set_icon_theme_path) {
      api_.app_indicator_set_icon_theme_path(indicator_, themePathString.c_str());
    }

    if (api_.app_indicator_set_icon_full) {
      api_.app_indicator_set_icon_full(indicator_, iconName.c_str(), iconName.c_str());
    } else if (api_.app_indicator_set_icon) {
      api_.app_indicator_set_icon(indicator_, iconName.c_str());
    } else {
      return;
    }

    currentIconKey_ = iconKey;
  }

  void ApplyTick(const lan::host_shell::NativeShellRuntimeLoopResult& tick) {
    const auto status = TranslateWide(tick.tracker.statusViewModel.statusText);
    const auto detail = TranslateWide(tick.tracker.statusViewModel.detailText);
    const auto badge = TranslateWide(tick.tracker.trayIconViewModel.statusBadge);
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
      detailLine += stableHealthy ? std::string(" | ") + TranslateWide(L"Healthy")
                                  : std::string(" | ") + TranslateWide(L"Health degraded");
    } else {
      detailLine += std::string(" | ") + TranslateWide(L"Service stopped");
    }

    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(statusItem_), status.c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(detailItem_), detailLine.c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(refreshDashboardItem_),
                                 canRefreshDashboard ? TranslateWide(L"Refresh Dashboard").c_str()
                                                     : TranslateWide(L"Refresh Dashboard (service stopped)").c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(openViewerItem_), ViewerLabel(viewers).c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(startServerItem_),
                                 canStartServer ? TranslateWide(L"Start Sharing Service").c_str()
                                                : TranslateWide(L"Start Sharing Service (already running)").c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(stopServerItem_),
                                 canStopServer ? TranslateWide(L"Stop Sharing Service").c_str()
                                               : TranslateWide(L"Stop Sharing Service (not running)").c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(openDiagnosticsItem_),
                                 canOpenDiagnostics ? TranslateWide(L"Open Diagnostics Folder").c_str()
                                                    : TranslateWide(L"Open Diagnostics Folder (service stopped)").c_str());
    api_.gtk_menu_item_set_label(reinterpret_cast<GtkMenuItem*>(exportDiagnosticsItem_),
                                 canExportDiagnostics ? TranslateWide(L"Export Diagnostics Snapshot").c_str()
                                                      : TranslateWide(L"Export Diagnostics Snapshot (no live data yet)").c_str());

    api_.gtk_widget_set_sensitive(openDashboardItem_, stableRunning ? 1 : 0);
    api_.gtk_widget_set_sensitive(refreshDashboardItem_, canRefreshDashboard ? 1 : 0);
    api_.gtk_widget_set_sensitive(openViewerItem_, canOpenViewer ? 1 : 0);
    api_.gtk_widget_set_sensitive(startServerItem_, canStartServer ? 1 : 0);
    api_.gtk_widget_set_sensitive(stopServerItem_, canStopServer ? 1 : 0);
    api_.gtk_widget_set_sensitive(openDiagnosticsItem_, canOpenDiagnostics ? 1 : 0);
    api_.gtk_widget_set_sensitive(exportDiagnosticsItem_, canExportDiagnostics ? 1 : 0);
    UpdateIndicatorIcon(tick);
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
      if (err.empty()) err = TranslateWide(L"The requested action failed.");
      Notify("Action failed", err);
    } else if (notifyOnSuccess && !successTitle.empty()) {
      Notify(successTitle, successBody.empty() ? TranslateWide(L"The action completed successfully.") : successBody);
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
      if (err.empty()) err = TranslateWide(L"Failed to export diagnostics snapshot.");
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
  std::filesystem::path iconRoot_;
  std::string currentIconKey_;
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
  const auto executableDir = lan::platform::ExecutableDir(argv[0]);
  bool diagnosticsDirExplicit = false;
  bool serverExecutableExplicit = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc) endpoint.host = argv[++i];
    else if (arg == "--port" && i + 1 < argc) endpoint.port = std::atoi(argv[++i]);
    else if (arg == "--room" && i + 1 < argc) { const char* value = argv[++i]; actionConfig.room.assign(value, value + std::strlen(value)); }
    else if (arg == "--token" && i + 1 < argc) { const char* value = argv[++i]; actionConfig.token.assign(value, value + std::strlen(value)); }
    else if (arg == "--diagnostics-dir" && i + 1 < argc) {
      actionConfig.diagnosticsDir = argv[++i];
      diagnosticsDirExplicit = true;
    }
    else if (arg == "--server-executable" && i + 1 < argc) {
      actionConfig.serverExecutable = argv[++i];
      serverExecutableExplicit = true;
    }
    else if (arg == "--server-arg" && i + 1 < argc) actionConfig.serverArguments.emplace_back(argv[++i]);
  }
  if (!diagnosticsDirExplicit) {
    actionConfig.diagnosticsDir = ResolveLinuxStateDir() / "out" / "diagnostics";
  }
  if (!serverExecutableExplicit) {
    const auto defaultServerExecutable = ResolveLinuxDefaultServerExecutable(executableDir);
    if (!defaultServerExecutable.empty()) {
      actionConfig.serverExecutable = defaultServerExecutable;
    }
  }
  actionConfig.host = endpoint.host;
  actionConfig.port = endpoint.port;
  actionConfig.localeCode = CurrentLocaleCode();

  GtkApi api;
  std::string err;
  if (!LoadGtkApi(api, err)) {
    std::cerr << err << "\n";
    return 1;
  }

  auto controller = lan::host_shell::NativeShellActionController(std::move(actionConfig));
  auto loop = lan::host_shell::NativeShellRuntimeLoop(lan::host_shell::MakeNativeShellPollFunction(endpoint),
                                                      controller.Platform());
  const auto iconRoot = DiscoverLinuxIconRoot(lan::platform::ExecutableDir(argv[0]));

  LinuxTrayApp app(api, std::move(controller), std::move(loop), iconRoot);
  if (!app.Initialize(err)) {
    std::cerr << err << "\n";
    return 1;
  }
  return app.Run();
}
