#pragma once

#include <windows.h>

#include "../core/runtime/desktop_shell_presenter.h"
#include "../core/runtime/host_shell_lifecycle_coordinator.h"
#include "../core/runtime/shell_chrome_presenter.h"

class MainWindow;

namespace lan::desktop {

inline constexpr UINT_PTR kHostRuntimeTimerId = 1;
inline constexpr UINT kHostAppTrayIconMessage = WM_APP + 4;
inline constexpr UINT_PTR kHostTrayIconUid = 1;

class ShellEffectExecutor {
public:
    static void ApplyHostShellLifecyclePlan(MainWindow& window,
                                            const lan::runtime::HostShellLifecyclePlan& plan);
    static void ExecuteTrayShellCommand(MainWindow& window,
                                        const lan::runtime::TrayShellCommandRoute& route);
    static void ExecuteDesktopShellCommand(MainWindow& window,
                                           const lan::runtime::DesktopShellCommandRoute& route);

    static void CreateTrayIcon(MainWindow& window);
    static void RemoveTrayIcon(MainWindow& window);
    static void UpdateTrayIcon(MainWindow& window);
    static void ShowTrayMenu(MainWindow& window);
};

} // namespace lan::desktop
