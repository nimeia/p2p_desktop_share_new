#pragma once

#include <filesystem>
#include <string>
#include <memory>
#include <atomic>
#include <array>
#include <vector>
#include <windows.h>

#include "ServerController.h"
#include "AdminBackend.h"
#include "WebViewHost.h"
#include "WebViewShellAdapter.h"
#include "core/runtime/desktop_runtime_snapshot.h"
#include "core/runtime/desktop_shell_presenter.h"
#include "core/runtime/desktop_layout_presenter.h"
#include "core/runtime/desktop_edit_session_presenter.h"
#include "core/runtime/shell_chrome_presenter.h"
#include "core/runtime/admin_shell_coordinator.h"
#include "core/runtime/host_runtime_coordinator.h"
#include "core/runtime/host_action_coordinator.h"
#include "core/runtime/host_session_coordinator.h"
#include "core/runtime/host_observability_coordinator.h"
#include "core/runtime/admin_view_model_assembler.h"
#include "core/runtime/diagnostics_view_model_assembler.h"
#include "platform/abstraction/platform_service_facade.h"

namespace fs = std::filesystem;

class DesktopHostPageBuilders;
namespace lan::desktop { class ShellEffectExecutor; }

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
    friend class DesktopHostPageBuilders;
    friend class lan::desktop::ShellEffectExecutor;

    enum class UiPage {
        Dashboard,
        Setup,
        Network,
        Sharing,
        Monitor,
        Diagnostics,
        Settings,
    };

    enum class WebViewSurfaceMode {
        Hidden,
        HostPreview,
        HtmlAdminPreview,
    };

    using LogEntry = lan::runtime::HostObservabilityLogEntry;

    using DashboardSuggestionKind = lan::runtime::AdminDashboardSuggestionKind;

    void OnCreate();
    void OnSize(int width, int height);
    void OnCommand(int id);
    void OnDestroy();

    void ExecuteDesktopShellCommand(const lan::runtime::DesktopShellCommandRoute& route);
    lan::runtime::DesktopLayoutStateInput BuildDesktopLayoutStateInput() const;
    void ApplyDesktopPageVisibility(const lan::runtime::DesktopPageVisibility& visibility);
    void ApplyNativeCommandButtonPolicy(const lan::runtime::NativeCommandButtonPolicy& policy);
    void ApplyDashboardButtonPolicy(const lan::runtime::DashboardButtonPolicy& policy);
    void ApplyNetworkButtonPolicy(const lan::runtime::NetworkButtonPolicy& policy);

    void StartServer();
    void StopServer();
    void RestartServer();
    void ExecuteHostAction(lan::runtime::HostActionKind kind);
    void ApplyHostActionResult(const lan::runtime::HostActionResult& result);
    lan::runtime::HostActionContext BuildHostActionContext() const;
    lan::runtime::HostSessionState BuildHostSessionState() const;
    lan::runtime::AdminViewModelInput BuildAdminViewModelInput() const;
    void ApplyHostSessionState(const lan::runtime::HostSessionState& state, bool updateControls = true);
    void ApplyAdminShellSessionRequest(const lan::runtime::AdminShellSessionRequest& request);
    void ApplyAdminShellHotspotRequest(const lan::runtime::AdminShellHotspotRequest& request);
    lan::runtime::DesktopEditSessionDraft BuildDesktopEditSessionDraftFromControls() const;
    lan::runtime::DesktopEditSessionInput BuildDesktopEditSessionInput() const;
    void ApplyDesktopEditSessionDraftToControls(const lan::runtime::DesktopEditSessionDraft& draft);
    lan::runtime::HostSessionMutationResult SyncHostSessionStateFromControls();
    void ApplyDesktopEditSessionViewModel(const lan::runtime::DesktopEditSessionViewModel& viewModel);
    lan::runtime::HostActionOperation PerformStartServerAction();
    lan::runtime::HostActionOperation PerformStopServerAction();
    lan::runtime::HostActionOperation PerformOpenHostPageAction();
    lan::runtime::HostActionOperation PerformOpenViewerPageAction();
    lan::runtime::HostActionOperation PerformEnsureShareArtifactsAction(const lan::runtime::HostActionArtifactRequest& request,
                                                                       lan::runtime::HostActionArtifactPaths& paths);
    lan::runtime::HostActionOperation PerformOpenPathAction(const fs::path& path);

    void RefreshHostRuntime();
    void GenerateRoomToken();
    void EnsureHotspotDefaults();

    void StartHotspot();
    void StopHotspot();
    void OpenWifiDirectPairing();
    void OpenSystemHotspotSettings();
    void OpenFirewallSettings();
    void RunNetworkDiagnostics();
    void CheckWebViewRuntime();
    void TrustLocalCertificate();
    void ExportRemoteProbeGuide();

    void OpenHostPage();
    void StartServiceOnly();
    void StartAndOpenHost();
    void OpenViewerPage();
    void CopyHostUrl();
    void CopyViewerUrl();
    void ShowQr();
    void ShowShareWizard();
    void ExportShareBundle();
    void QuickFixNetwork();
    void QuickFixCertificate();
    void QuickFixSharing();
    void QuickFixHandoff();
    void QuickFixHotspot();
    void RunDesktopSelfCheck();
    void RefreshDiagnosticsBundle();
    void OpenDiagnosticsReport();
    void OpenOutputFolder();

    bool WriteShareArtifacts(fs::path* shareCardPath = nullptr,
                             fs::path* shareWizardPath = nullptr,
                             fs::path* bundleJsonPath = nullptr,
                             fs::path* desktopSelfCheckPath = nullptr);
    std::wstring BuildWifiDirectSessionAlias() const;
    lan::runtime::DesktopRuntimeSnapshot BuildDesktopRuntimeSnapshot(bool liveReady = true) const;

    lan::desktop::WebViewShellState BuildWebViewShellState() const;
    lan::desktop::WebViewShellContext BuildWebViewShellContext() const;
    lan::desktop::WebViewShellHooks BuildWebViewShellHooks();
    void ApplyWebViewShellState(const lan::desktop::WebViewShellState& state);
    void RestoreWebViewShellState();
    void NavigateHostInWebView();
    void NavigateHtmlAdminInWebView();
    void EnsureWebViewInitialized();
    void RefreshHtmlAdminPreview();
    void PublishAdminShellRuntime();
    void HandleAdminShellMessage(std::wstring_view payload);
    lan::runtime::AdminShellCoordinatorHooks BuildAdminShellCoordinatorHooks();

    void AppendLog(std::wstring_view line);
    void RefreshShellFallback();
    void RefreshDashboard();
    void RefreshSessionSetup();
    void RefreshNetworkPage();
    void RefreshSharingPage();
    void RefreshMonitorPage();
    void RefreshDiagnosticsPage();
    void RefreshSettingsPage();
    void UpdateUiState();
    void RefreshShareInfo();
    void ApplyHostRuntimeRefresh(const lan::runtime::HostRuntimeRefreshResult& refresh);
    void HandleWebViewMessage(std::wstring_view payload);
    void SetPage(UiPage page);
    void UpdatePageVisibility();
    void ExecuteDashboardSuggestionFix(std::size_t index);
    void ExecuteDashboardSuggestionInfo(std::size_t index);
    void SelectNetworkCandidate(std::size_t index);
    lan::runtime::HostObservabilityState BuildHostObservabilityState() const;
    void ApplyHostObservabilityState(const lan::runtime::HostObservabilityState& state);
    void AddTimelineEvent(std::wstring_view eventText);
    void RefreshFilteredLogs();
    void CreateTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon();
    void ShowTrayMenu();
    void MinimizeToTray(bool showBalloon);
    void RestoreFromTray();
    lan::runtime::ShellChromeStateInput BuildShellChromeStateInput() const;
    void ApplyShellChromeStatusViewModel(const lan::runtime::ShellChromeStatusViewModel& viewModel);
    void ExecuteTrayShellCommand(const lan::runtime::TrayShellCommandRoute& route);
    lan::runtime::HostShellLifecycleInput BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent event, bool showBalloon = false) const;
    void ApplyHostShellLifecyclePlan(const lan::runtime::HostShellLifecyclePlan& plan);

    void HandleRuntimeTick();
    void KickPoll();
    void HandlePollResult(DWORD status, std::size_t rooms, std::size_t viewers);

    std::wstring BuildHostUrlLocal() const;
    std::wstring BuildViewerUrl() const;
    void BuildHandoffSummary(std::wstring* state, std::wstring* label, std::wstring* detail) const;
    fs::path AppDir() const;
    fs::path AdminUiDir() const;
    bool PreferHtmlAdminUi() const;
    bool IsHtmlAdminActive() const;
    std::wstring AdminTabNameForPage(UiPage page) const;
    bool TrySetPageFromAdminTab(std::wstring_view tab);

private:
    HWND m_hwnd = nullptr;

    // server + webview
    std::unique_ptr<AdminBackend> m_adminBackend;
    std::unique_ptr<ServerController> m_server;
    std::unique_ptr<lan::platform::PlatformServiceFacade> m_platformServices;
    WebViewHost m_webview;

    // state
    std::wstring m_hostIp = L"";
    std::wstring m_bindAddress = L"0.0.0.0";
    int m_port = 9443;
    std::wstring m_room = L"";
    std::wstring m_token = L"";

    std::wstring m_logs;
    std::wstring m_timelineText;
    std::wstring m_lastErrorSummary = L"";
    bool m_shareWizardOpened = false;
    bool m_handoffStarted = false;
    bool m_handoffDelivered = false;
    bool m_exitRequested = false;
    bool m_trayIconAdded = false;
    bool m_trayBalloonShown = false;
    std::vector<LogEntry> m_logEntries;
    bool m_viewerUrlCopied = false;
    bool m_shareCardExported = false;
    bool m_adminShellReady = false;
    bool m_htmlAdminNavigated = false;
    WebViewSurfaceMode m_webviewMode = WebViewSurfaceMode::Hidden;
    bool m_shellStartButtonEnabled = true;

    std::atomic<bool> m_polling{false};
    lan::runtime::HostRuntimeTickState m_runtimeTickState{};
    UiPage m_currentPage = UiPage::Dashboard;

    // Shared left pane controls
    HWND m_navDashboardBtn = nullptr;
    HWND m_navSetupBtn = nullptr;
    HWND m_navNetworkBtn = nullptr;
    HWND m_navSharingBtn = nullptr;
    HWND m_navMonitorBtn = nullptr;
    HWND m_navDiagnosticsBtn = nullptr;
    HWND m_navSettingsBtn = nullptr;

    // Dashboard page controls
    HWND m_dashboardIntro = nullptr;
    HWND m_dashboardStatusCard = nullptr;
    HWND m_dashboardPrimaryBtn = nullptr;
    HWND m_dashboardContinueBtn = nullptr;
    HWND m_dashboardWizardBtn = nullptr;
    HWND m_dashboardNetworkCard = nullptr;
    HWND m_dashboardServiceCard = nullptr;
    HWND m_dashboardShareCard = nullptr;
    HWND m_dashboardHealthCard = nullptr;
    HWND m_dashboardSuggestionsLabel = nullptr;
    std::array<HWND, 4> m_dashboardSuggestionText{};
    std::array<HWND, 4> m_dashboardSuggestionFixBtn{};
    std::array<HWND, 4> m_dashboardSuggestionInfoBtn{};
    std::array<HWND, 4> m_dashboardSuggestionSetupBtn{};
    std::array<DashboardSuggestionKind, 4> m_dashboardSuggestionKinds{};
    std::array<std::wstring, 4> m_dashboardSuggestionDetails{};

    // Sharing page controls
    HWND m_sharingTitle = nullptr;
    HWND m_accessEntryCard = nullptr;
    HWND m_qrCard = nullptr;
    HWND m_accessGuideCard = nullptr;
    HWND m_btnCopyHostUrl = nullptr;
    HWND m_btnCopyViewerUrl = nullptr;
    HWND m_btnOpenHostBrowser = nullptr;
    HWND m_btnOpenViewerBrowser = nullptr;
    HWND m_btnSaveQrImage = nullptr;
    HWND m_btnFullscreenQr = nullptr;
    HWND m_btnOpenShareCard = nullptr;
    HWND m_btnOpenShareWizard = nullptr;
    HWND m_btnOpenBundleFolder = nullptr;
    HWND m_btnExportOfflineZip = nullptr;

    // Monitor page controls
    HWND m_monitorTitle = nullptr;
    std::array<HWND, 5> m_monitorMetricCards{};
    HWND m_monitorTimelineLabel = nullptr;
    HWND m_monitorTimelineBox = nullptr;
    HWND m_monitorTabHealth = nullptr;
    HWND m_monitorTabConnections = nullptr;
    HWND m_monitorTabLogs = nullptr;
    HWND m_monitorDetailBox = nullptr;

    // Diagnostics page controls
    HWND m_diagPageTitle = nullptr;
    HWND m_diagChecklistCard = nullptr;
    HWND m_diagActionsCard = nullptr;
    HWND m_diagExportCard = nullptr;
    HWND m_diagFilesCard = nullptr;
    HWND m_diagLogSearch = nullptr;
    HWND m_diagLevelFilter = nullptr;
    HWND m_diagSourceFilter = nullptr;
    HWND m_diagLogViewer = nullptr;
    HWND m_btnDiagOpenOutput = nullptr;
    HWND m_btnDiagOpenReport = nullptr;
    HWND m_btnDiagExportZip = nullptr;
    HWND m_btnDiagCopyPath = nullptr;
    HWND m_btnDiagRefreshBundle = nullptr;
    HWND m_btnDiagCopyLogs = nullptr;
    HWND m_btnDiagSaveLogs = nullptr;

    // Settings page controls
    HWND m_settingsTitle = nullptr;
    HWND m_settingsIntro = nullptr;
    HWND m_settingsGeneralCard = nullptr;
    HWND m_settingsServiceCard = nullptr;
    HWND m_settingsNetworkCard = nullptr;
    HWND m_settingsSharingCard = nullptr;
    HWND m_settingsLoggingCard = nullptr;
    HWND m_settingsAdvancedCard = nullptr;
    HWND m_settingsCurrentStateCard = nullptr;

    // Network page controls
    HWND m_networkTitle = nullptr;
    HWND m_networkSummaryCard = nullptr;
    HWND m_btnRefreshNetwork = nullptr;
    HWND m_btnManualSelectIp = nullptr;
    HWND m_adapterListLabel = nullptr;
    std::array<HWND, 4> m_networkAdapterCards{};
    std::array<HWND, 4> m_networkAdapterSelectBtns{};
    HWND m_hotspotGroup = nullptr;
    HWND m_hotspotStatusCard = nullptr;
    HWND m_hotspotSsidLabel = nullptr;
    HWND m_hotspotPwdLabel = nullptr;
    HWND m_hotspotSsidEdit = nullptr;
    HWND m_hotspotPwdEdit = nullptr;
    HWND m_btnAutoHotspot = nullptr;
    HWND m_btnStartHotspot = nullptr;
    HWND m_btnStopHotspot = nullptr;
    HWND m_btnOpenHotspotSettings = nullptr;
    HWND m_wifiDirectGroup = nullptr;
    HWND m_wifiDirectCard = nullptr;
    HWND m_btnPairWifiDirect = nullptr;
    HWND m_btnOpenConnectedDevices = nullptr;
    HWND m_btnOpenPairingHelp = nullptr;

    // Setup page controls
    HWND m_stepInfo = nullptr;
    HWND m_setupTitle = nullptr;
    HWND m_sessionGroup = nullptr;
    HWND m_serviceGroup = nullptr;
    HWND m_templateLabel = nullptr;
    HWND m_templateCombo = nullptr;

    HWND m_ipLabel = nullptr;
    HWND m_ipValue = nullptr;
    HWND m_btnRefreshIp = nullptr;

    HWND m_netCapsText = nullptr;

    HWND m_hotspotLabel = nullptr;

    HWND m_bindLabel = nullptr;
    HWND m_bindEdit = nullptr;
    HWND m_sanIpLabel = nullptr;
    HWND m_sanIpValue = nullptr;
    HWND m_advancedToggle = nullptr;

    HWND m_portLabel = nullptr;
    HWND m_portEdit = nullptr;

    HWND m_roomLabel = nullptr;
    HWND m_roomEdit = nullptr;

    HWND m_tokenLabel = nullptr;
    HWND m_tokenEdit = nullptr;
    HWND m_btnGenerate = nullptr;

    HWND m_btnStart = nullptr;
    HWND m_btnStop = nullptr;
    HWND m_btnRestart = nullptr;
    HWND m_btnServiceOnly = nullptr;
    HWND m_btnStartAndOpenHost = nullptr;

    HWND m_statusText = nullptr;
    HWND m_statsText = nullptr;
    HWND m_webStateText = nullptr;
    HWND m_diagSummaryLabel = nullptr;
    HWND m_diagSummaryBox = nullptr;
    HWND m_firstActionsLabel = nullptr;
    HWND m_firstActionsBox = nullptr;
    HWND m_shareInfoLabel = nullptr;
    HWND m_shareInfoBox = nullptr;
    HWND m_sessionSummaryLabel = nullptr;
    HWND m_sessionSummaryBox = nullptr;

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
    HWND m_hostPreviewPlaceholder = nullptr;
    HWND m_runtimeInfoCard = nullptr;
    HWND m_shellFallbackBox = nullptr;
    HWND m_shellRetryBtn = nullptr;
    HWND m_shellStartBtn = nullptr;
    HWND m_shellStartHostBtn = nullptr;
    HWND m_shellOpenHostBtn = nullptr;

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
    std::array<std::wstring, 4> m_networkCandidateIps{};

    // Default settings snapshot
    int m_defaultPort = 9443;
    std::wstring m_defaultBindAddress = L"0.0.0.0";
    std::wstring m_roomRule = L"random-6";
    std::wstring m_tokenRule = L"random-16";
    std::wstring m_defaultServerExePath = L"";
    std::wstring m_defaultWwwPath = L"";
    std::wstring m_defaultCertDir = L"";
    std::wstring m_defaultLaunchArgs = L"--bind {bind} --port {port} --san-ip {host_ip}";
    std::wstring m_defaultIpStrategy = L"prefer-private-wifi";
    int m_autoDetectFrequencySec = 15;
    std::wstring m_hotspotPasswordRule = L"windows-suggested";
    std::wstring m_defaultViewerOpenMode = L"app-window-preferred";
    bool m_autoCopyViewerLink = true;
    bool m_autoGenerateQr = true;
    bool m_autoExportBundle = true;
    std::wstring m_logLevel = L"info";
    std::wstring m_outputDir = L"";
    int m_diagnosticsRetentionDays = 7;
    bool m_saveStdStreams = true;
    std::wstring m_certBypassPolicy = L"allow-loopback-and-private-lan-self-signed";
    std::wstring m_webViewBehavior = L"embedded-when-available";
    std::wstring m_startupHook = L"(not configured)";
};
