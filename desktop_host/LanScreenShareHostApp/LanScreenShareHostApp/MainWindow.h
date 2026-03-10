#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <atomic>
#include <windows.h>

#include "ServerController.h"
#include "WebViewHost.h"

namespace fs = std::filesystem;

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create();
    void Show();
    void Hide();
    HWND GetHwnd() const { return m_hwnd; }

    static MainWindow* GetInstance(HWND hwnd);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(int id);
    void OnDestroy();

    void StartServer();
    void StopServer();

    void RefreshHostIp();
    void RefreshNetworkCapabilities();
    void RefreshHotspotState();
    void GenerateRoomToken();
    void EnsureHotspotDefaults();

    void StartHotspot();
    void StopHotspot();
    void OpenWifiDirectPairing();
    void OpenSystemHotspotSettings();

    void OpenHostPage();
    void OpenViewerPage();
    void CopyViewerUrl();
    void ShowQr();
    void ShowShareWizard();
    void ExportShareBundle();
    void RunDesktopSelfCheck();
    void RefreshDiagnosticsBundle();
    void OpenDiagnosticsReport();
    void OpenOutputFolder();

    bool WriteShareArtifacts(fs::path* shareCardPath = nullptr,
                             fs::path* shareWizardPath = nullptr,
                             fs::path* bundleJsonPath = nullptr,
                             fs::path* desktopSelfCheckPath = nullptr);
    std::wstring BuildWifiDirectSessionAlias() const;

    void NavigateHostInWebView();

    void AppendLog(std::wstring_view line);
    void UpdateUiState();
    void RefreshShareInfo();
    void HandleWebViewMessage(std::wstring_view payload);

    void KickPoll();
    void HandlePollResult(DWORD status, std::size_t rooms, std::size_t viewers);

    std::wstring BuildHostUrlLocal() const;
    std::wstring BuildViewerUrl() const;
    fs::path AppDir() const;

private:
    HWND m_hwnd = nullptr;

    // server + webview
    std::unique_ptr<ServerController> m_server;
    WebViewHost m_webview;

    // state
    std::wstring m_hostIp = L"";
    std::wstring m_bindAddress = L"0.0.0.0";
    int m_port = 9443;
    std::wstring m_room = L"";
    std::wstring m_token = L"";

    std::wstring m_logs;

    std::atomic<bool> m_polling{false};

    // Left pane controls
    HWND m_stepInfo = nullptr;

    HWND m_ipLabel = nullptr;
    HWND m_ipValue = nullptr;
    HWND m_btnRefreshIp = nullptr;

    HWND m_netCapsText = nullptr;

    HWND m_hotspotLabel = nullptr;
    HWND m_hotspotSsidEdit = nullptr;
    HWND m_hotspotPwdEdit = nullptr;
    HWND m_btnStartHotspot = nullptr;
    HWND m_btnStopHotspot = nullptr;
    HWND m_btnPairWifiDirect = nullptr;
    HWND m_btnOpenHotspotSettings = nullptr;

    HWND m_bindLabel = nullptr;
    HWND m_bindEdit = nullptr;

    HWND m_portLabel = nullptr;
    HWND m_portEdit = nullptr;

    HWND m_roomLabel = nullptr;
    HWND m_roomEdit = nullptr;

    HWND m_tokenLabel = nullptr;
    HWND m_tokenEdit = nullptr;
    HWND m_btnGenerate = nullptr;

    HWND m_btnStart = nullptr;
    HWND m_btnStop = nullptr;

    HWND m_statusText = nullptr;
    HWND m_statsText = nullptr;
    HWND m_webStateText = nullptr;
    HWND m_diagSummaryLabel = nullptr;
    HWND m_diagSummaryBox = nullptr;
    HWND m_firstActionsLabel = nullptr;
    HWND m_firstActionsBox = nullptr;
    HWND m_shareInfoLabel = nullptr;
    HWND m_shareInfoBox = nullptr;

    HWND m_btnOpenHost = nullptr;
    HWND m_btnOpenViewer = nullptr;
    HWND m_btnCopyViewer = nullptr;
    HWND m_btnShowQr = nullptr;
    HWND m_btnShowWizard = nullptr;
    HWND m_btnExportBundle = nullptr;
    HWND m_btnRunSelfCheck = nullptr;
    HWND m_btnRefreshChecks = nullptr;
    HWND m_btnOpenDiagnostics = nullptr;
    HWND m_btnOpenFolder = nullptr;

    HWND m_logLabel = nullptr;
    HWND m_logBox = nullptr;

    std::wstring m_networkMode = L"";
    std::wstring m_hostPageState = L"idle";
    std::wstring m_hotspotSsid = L"";
    std::wstring m_hotspotPassword = L"";
    std::wstring m_hotspotStatus = L"stopped";
    bool m_wifiAdapterPresent = false;
    bool m_hotspotSupported = false;
    bool m_hotspotRunning = false;
    bool m_wifiDirectApiAvailable = false;
    std::size_t m_lastRooms = 0;
    std::size_t m_lastViewers = 0;
};
