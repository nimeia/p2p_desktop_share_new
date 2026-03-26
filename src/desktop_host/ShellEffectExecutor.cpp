#include "pch.h"
#include "ShellEffectExecutor.h"

#include "MainWindow.h"
#include "DesktopCommandIds.h"
#include "UrlUtil.h"
#include "core/i18n/localization.h"

namespace fs = std::filesystem;

namespace {

UINT TrayMenuFlags(bool enabled) {
    return MF_STRING | (enabled ? 0 : MF_GRAYED);
}

int GetTrayIconMetric(int metric, HWND hwnd) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    using GetSystemMetricsForDpiFn = int(WINAPI*)(int, UINT);

    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
        user32 ? GetProcAddress(user32, "GetDpiForWindow") : nullptr);
    const auto getSystemMetricsForDpi = reinterpret_cast<GetSystemMetricsForDpiFn>(
        user32 ? GetProcAddress(user32, "GetSystemMetricsForDpi") : nullptr);

    if (hwnd && getDpiForWindow && getSystemMetricsForDpi) {
        const UINT dpi = getDpiForWindow(hwnd);
        if (dpi > 0) {
            const int scaled = getSystemMetricsForDpi(metric, dpi);
            if (scaled > 0) return scaled;
        }
    }

    const int fallback = GetSystemMetrics(metric);
    return fallback > 0 ? fallback : 16;
}

UINT SelectTrayIconResourceId(const lan::runtime::ShellChromeStateInput& input) {
    if (input.attentionNeeded) return 103;
    if (input.viewerCount > 0) return 105;
    if (input.serverRunning && input.hostStateSharing) return 107;
    return 101;
}

UINT SelectTrayIconFallbackResourceId(UINT resourceId) {
    switch (resourceId) {
    case 103:
        return 104;
    case 105:
        return 106;
    case 107:
        return 108;
    case 101:
    default:
        return 102;
    }
}

HICON LoadTrayIconForState(const lan::runtime::ShellChromeStateInput& input, HWND hwnd) {
    const UINT resourceId = SelectTrayIconResourceId(input);
    const UINT fallbackId = SelectTrayIconFallbackResourceId(resourceId);
    const int width = GetTrayIconMetric(SM_CXSMICON, hwnd);
    const int height = GetTrayIconMetric(SM_CYSMICON, hwnd);
    const UINT flags = LR_DEFAULTCOLOR | LR_SHARED;
    HICON icon = static_cast<HICON>(LoadImageW(
        GetModuleHandle(nullptr), MAKEINTRESOURCEW(resourceId), IMAGE_ICON, width, height, flags));
    if (!icon) {
        icon = static_cast<HICON>(LoadImageW(
            GetModuleHandle(nullptr), MAKEINTRESOURCEW(fallbackId), IMAGE_ICON, width, height, flags));
    }
    return icon ? icon : LoadIcon(nullptr, IDI_APPLICATION);
}

} // namespace

namespace lan::desktop {

void ShellEffectExecutor::ApplyHostShellLifecyclePlan(MainWindow& window,
                                                      const lan::runtime::HostShellLifecyclePlan& plan) {
    if (!window.m_hwnd) return;

    if (plan.markExitRequested) {
        window.m_exitRequested = true;
    }
    if (plan.markTrayBalloonPending) {
        window.m_trayBalloonShown = false;
    }
    if (plan.setDashboardPage) {
        window.SetPage(MainWindow::UiPage::Dashboard);
    }
    if (plan.showWindow) {
        ShowWindow(window.m_hwnd, SW_SHOW);
    }
    if (plan.restoreWindow) {
        ShowWindow(window.m_hwnd, IsIconic(window.m_hwnd) ? SW_RESTORE : SW_SHOW);
        ShowWindow(window.m_hwnd, SW_RESTORE);
    }
    if (plan.updateWindow) {
        UpdateWindow(window.m_hwnd);
    }
    if (plan.setForeground) {
        SetForegroundWindow(window.m_hwnd);
    }
    if (plan.hideWindow) {
        ShowWindow(window.m_hwnd, SW_HIDE);
    }
    if (plan.ensureWebViewInitialized) {
        window.RestoreWebViewShellState();
    }
    if (plan.relayoutWindow) {
        RECT rc{};
        GetClientRect(window.m_hwnd, &rc);
        window.OnSize(rc.right, rc.bottom);
    }
    if (plan.refreshHostRuntime) {
        window.RefreshHostRuntime();
    }
    if (plan.createRuntimeTimer) {
        SetTimer(window.m_hwnd,
                 kHostRuntimeTimerId,
                 static_cast<UINT>(plan.timerIntervalMs > 0 ? plan.timerIntervalMs : 1000),
                 nullptr);
    }
    if (plan.createTrayIcon) {
        CreateTrayIcon(window);
    }
    if (plan.appendUiCreatedLog) {
        window.AppendLog(L"UI created");
    }
    if (plan.refreshShareInfo) {
        window.RefreshShareInfo();
    }
    if (plan.refreshDashboard) {
        window.RefreshDashboard();
    }
    if (plan.updateUiState) {
        window.UpdateUiState();
    }
    if (plan.refreshHtmlAdminPreview) {
        window.RefreshHtmlAdminPreview();
    }
    if (plan.refreshShellFallback) {
        window.RefreshShellFallback();
    }
    if (plan.applyNativeCommandButtonPolicy) {
        const bool running = window.m_server && window.m_server->IsRunning();
        window.ApplyNativeCommandButtonPolicy(
            lan::runtime::BuildNativeCommandButtonPolicy(
                {running, window.m_hotspotRunning, window.m_hotspotSupported, window.m_shellStartButtonEnabled}));
    }
    if (plan.updateTrayIcon) {
        UpdateTrayIcon(window);
    }
    if (plan.killTimer) {
        KillTimer(window.m_hwnd, kHostRuntimeTimerId);
    }
    if (plan.removeTrayIcon) {
        RemoveTrayIcon(window);
    }
    if (plan.stopServer) {
        window.StopServer();
    }
    if (plan.destroyWindow) {
        DestroyWindow(window.m_hwnd);
    }
}

void ShellEffectExecutor::ExecuteTrayShellCommand(MainWindow& window,
                                                  const lan::runtime::TrayShellCommandRoute& route) {
    switch (route.kind) {
    case lan::runtime::TrayShellCommandKind::OpenDashboard:
        window.RestoreFromTray();
        break;
    case lan::runtime::TrayShellCommandKind::StartSharing:
        window.RestoreFromTray();
        window.StartServer();
        break;
    case lan::runtime::TrayShellCommandKind::StopSharing:
        window.StopServer();
        break;
    case lan::runtime::TrayShellCommandKind::CopyViewerUrl:
        window.CopyViewerUrl();
        break;
    case lan::runtime::TrayShellCommandKind::ShowQr:
        window.ShowQr();
        break;
    case lan::runtime::TrayShellCommandKind::OpenShareWizard:
        window.ShowShareWizard();
        break;
    case lan::runtime::TrayShellCommandKind::ExitApp:
        ApplyHostShellLifecyclePlan(window,
            lan::runtime::CoordinateHostShellLifecycle(
                window.BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::TrayExitRequested)));
        break;
    case lan::runtime::TrayShellCommandKind::None:
    default:
        break;
    }
}

void ShellEffectExecutor::CreateTrayIcon(MainWindow& window) {
    if (!window.m_hwnd || window.m_trayIconAdded) return;

    const auto shellState = window.BuildShellChromeStateInput();

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = window.m_hwnd;
    nid.uID = kHostTrayIconUid;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = kHostAppTrayIconMessage;
    nid.hIcon = LoadTrayIconForState(shellState, window.m_hwnd);
    const auto tip = lan::i18n::TranslateNativeText(L"ViewMesh Host", window.m_localeCode);
    lstrcpynW(nid.szTip, tip.c_str(), _countof(nid.szTip));
    if (Shell_NotifyIconW(NIM_ADD, &nid)) {
        window.m_trayIconAdded = true;
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
        window.m_trayBalloonShown = true;
        UpdateTrayIcon(window);
    }
}

void ShellEffectExecutor::RemoveTrayIcon(MainWindow& window) {
    if (!window.m_hwnd || !window.m_trayIconAdded) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = window.m_hwnd;
    nid.uID = kHostTrayIconUid;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    window.m_trayIconAdded = false;
}

void ShellEffectExecutor::UpdateTrayIcon(MainWindow& window) {
    if (!window.m_hwnd || !window.m_trayIconAdded) return;

    const auto shellState = window.BuildShellChromeStateInput();
    const auto viewModel = lan::runtime::BuildTrayIconViewModel(shellState);

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = window.m_hwnd;
    nid.uID = kHostTrayIconUid;
    nid.uFlags = NIF_TIP | NIF_SHOWTIP | NIF_ICON;
    nid.hIcon = LoadTrayIconForState(shellState, window.m_hwnd);
    const auto tooltip = lan::i18n::TranslateNativeText(viewModel.tooltip, window.m_localeCode);
    lstrcpynW(nid.szTip, tooltip.c_str(), _countof(nid.szTip));
    if (viewModel.showBalloon) {
        nid.uFlags |= NIF_INFO;
        const auto title = lan::i18n::TranslateNativeText(viewModel.balloonTitle, window.m_localeCode);
        const auto body = lan::i18n::TranslateNativeText(viewModel.balloonText, window.m_localeCode);
        lstrcpynW(nid.szInfoTitle, title.c_str(), _countof(nid.szInfoTitle));
        lstrcpynW(nid.szInfo, body.c_str(), _countof(nid.szInfo));
        nid.dwInfoFlags = NIIF_INFO;
        window.m_trayBalloonShown = true;
    }
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void ShellEffectExecutor::ShowTrayMenu(MainWindow& window) {
    if (!window.m_hwnd || !window.m_trayIconAdded) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    const auto viewModel = lan::runtime::BuildTrayMenuViewModel(window.BuildShellChromeStateInput());
    const auto locale = window.m_localeCode;

    const auto openDashboard = lan::i18n::TranslateNativeText(L"Open Dashboard", locale);
    AppendMenuW(menu, MF_STRING, ID_TRAY_OPEN_DASHBOARD, openDashboard.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    if (viewModel.showStopSharing) {
        const auto stopSharing = lan::i18n::TranslateNativeText(L"Stop Sharing Service", locale);
        AppendMenuW(menu, MF_STRING, ID_TRAY_STOP_SHARING, stopSharing.c_str());
    } else {
        const auto startSharing = lan::i18n::TranslateNativeText(L"Start Sharing Service", locale);
        AppendMenuW(menu, MF_STRING, ID_TRAY_START_SHARING, startSharing.c_str());
    }
    const auto copyViewer = lan::i18n::TranslateNativeText(L"Copy Viewer URL", locale);
    const auto showQr = lan::i18n::TranslateNativeText(L"Show QR / Share Card", locale);
    const auto openWizard = lan::i18n::TranslateNativeText(L"Open Share Wizard", locale);
    const auto exitText = lan::i18n::TranslateNativeText(L"Quit", locale);
    AppendMenuW(menu, TrayMenuFlags(viewModel.copyViewerUrlEnabled), ID_TRAY_COPY_VIEWER_URL, copyViewer.c_str());
    AppendMenuW(menu, TrayMenuFlags(viewModel.showQrEnabled), ID_TRAY_SHOW_QR, showQr.c_str());
    AppendMenuW(menu, TrayMenuFlags(viewModel.openShareWizardEnabled), ID_TRAY_OPEN_SHARE_WIZARD, openWizard.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, exitText.c_str());

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(window.m_hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, window.m_hwnd, nullptr);
    DestroyMenu(menu);
}

void ShellEffectExecutor::ExecuteDesktopShellCommand(MainWindow& window,
                                                     const lan::runtime::DesktopShellCommandRoute& route) {
    using lan::runtime::DesktopShellCommandKind;
    using lan::runtime::DesktopShellPage;

    switch (route.kind) {
    case DesktopShellCommandKind::NavigatePage:
        switch (route.page) {
        case DesktopShellPage::Dashboard:
            window.SetPage(MainWindow::UiPage::Dashboard);
            break;
        case DesktopShellPage::Setup:
            window.SetPage(MainWindow::UiPage::Setup);
            break;
        case DesktopShellPage::Network:
            window.SetPage(MainWindow::UiPage::Network);
            break;
        case DesktopShellPage::Sharing:
            window.SetPage(MainWindow::UiPage::Sharing);
            break;
        case DesktopShellPage::Monitor:
            window.SetPage(MainWindow::UiPage::Monitor);
            break;
        case DesktopShellPage::Diagnostics:
            window.SetPage(MainWindow::UiPage::Diagnostics);
            break;
        case DesktopShellPage::Settings:
            window.SetPage(MainWindow::UiPage::Settings);
            break;
        case DesktopShellPage::None:
        default:
            break;
        }
        break;
    case DesktopShellCommandKind::RetryShell:
        window.NavigateHtmlAdminInWebView();
        window.RefreshShellFallback();
        break;
    case DesktopShellCommandKind::ShellOpenHost:
        if (window.m_server && !window.m_server->IsRunning()) {
            window.StartAndOpenHost();
        } else {
            window.OpenHostPage();
        }
        break;
    case DesktopShellCommandKind::RefreshFilteredLogs:
        window.RefreshFilteredLogs();
        break;
    case DesktopShellCommandKind::EditSessionDraftChanged:
        window.RefreshSessionSetup();
        break;
    case DesktopShellCommandKind::ApplySessionTemplate: {
        const int sel = window.m_templateCombo ? static_cast<int>(SendMessageW(window.m_templateCombo, CB_GETCURSEL, 0, 0)) : 0;
        const auto baseState = lan::runtime::ApplyDesktopEditSessionDraft(window.BuildHostSessionState(), window.BuildDesktopEditSessionDraftFromControls()).state;
        const auto draft = lan::runtime::ApplyDesktopSessionTemplate(baseState, sel, sel);
        window.ApplyDesktopEditSessionDraftToControls(draft);
        window.RefreshSessionSetup();
        break;
    }
    case DesktopShellCommandKind::RefreshHostRuntime:
        window.RefreshHostRuntime();
        break;
    case DesktopShellCommandKind::EnsureHotspotDefaults:
        window.EnsureHotspotDefaults();
        window.RefreshNetworkPage();
        break;
    case DesktopShellCommandKind::GenerateRoomToken:
        window.GenerateRoomToken();
        break;
    case DesktopShellCommandKind::ExecuteHostAction:
        window.ExecuteHostAction(route.hostAction);
        break;
    case DesktopShellCommandKind::CopyHostUrl:
        window.CopyHostUrl();
        break;
    case DesktopShellCommandKind::CopyViewerUrl:
        window.CopyViewerUrl();
        break;
    case DesktopShellCommandKind::SaveQrImage:
        window.ExportShareBundle();
        window.AppendLog(L"Exported sharing bundle for QR save");
        break;
    case DesktopShellCommandKind::DiagnosticsExportZip:
        window.ExportShareBundle();
        window.AppendLog(L"Offline zip export placeholder: bundle refreshed");
        break;
    case DesktopShellCommandKind::CopyDiagnosticsPath: {
        const auto path = (window.AppDir() / L"out" / L"share_bundle").wstring();
        if (urlutil::SetClipboardText(window.m_hwnd, path)) window.AppendLog(L"Diagnostics path copied");
        break;
    }
    case DesktopShellCommandKind::CopyDiagnosticsLogs:
        if (window.m_diagLogViewer) {
            wchar_t buf[32768]{};
            GetWindowTextW(window.m_diagLogViewer, buf, _countof(buf));
            urlutil::SetClipboardText(window.m_hwnd, buf);
        }
        break;
    case DesktopShellCommandKind::SaveDiagnosticsLogs:
        window.ExportShareBundle();
        window.AppendLog(L"Saved logs via bundle refresh placeholder");
        break;
    case DesktopShellCommandKind::StartHotspot:
        window.StartHotspot();
        break;
    case DesktopShellCommandKind::StopHotspot:
        window.StopHotspot();
        break;
    case DesktopShellCommandKind::OpenWifiDirectPairing:
        window.OpenWifiDirectPairing();
        break;
    case DesktopShellCommandKind::OpenSystemHotspotSettings:
        window.OpenSystemHotspotSettings();
        break;
    case DesktopShellCommandKind::OpenPairingHelp:
        MessageBoxW(window.m_hwnd,
            L"1. Open Connected Devices.\r\n2. Pick the target device.\r\n3. Confirm pairing in Windows UI.\r\n4. Keep both devices on the same local link, then use the Viewer URL.",
            L"Pairing Help",
            MB_OK | MB_ICONINFORMATION);
        break;
    case DesktopShellCommandKind::SelectNetworkCandidate:
        window.SelectNetworkCandidate(route.index);
        break;
    case DesktopShellCommandKind::DashboardSuggestionFix:
        window.ExecuteDashboardSuggestionFix(route.index);
        break;
    case DesktopShellCommandKind::DashboardSuggestionInfo:
        window.ExecuteDashboardSuggestionInfo(route.index);
        break;
    case DesktopShellCommandKind::DashboardSuggestionSetup:
        window.SetPage(MainWindow::UiPage::Setup);
        break;
    case DesktopShellCommandKind::None:
    default:
        break;
    }
}

} // namespace lan::desktop
