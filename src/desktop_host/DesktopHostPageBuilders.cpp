#include "pch.h"
#include "DesktopHostPageBuilders.h"

#include "MainWindow.h"
#include "DesktopCommandIds.h"
#include "NativeControlFactory.h"

#include <array>
#include <string>

namespace {

struct NativePageLayoutMetrics {
    static constexpr int leftPaneWidth = 720;
    static constexpr int pad = 10;
    static constexpr int rowHeight = 22;
    static constexpr int buttonHeight = 28;
    static constexpr int labelWidth = 95;
    static constexpr int setupTop = pad + 42;
};

} // namespace

class DesktopHostPageBuilderImpl {
public:
    explicit DesktopHostPageBuilderImpl(MainWindow& window)
        : window_(window),
          factory_(window.m_hwnd, GetModuleHandle(nullptr)) {
    }

    void BuildAll() {
        BuildNavigation();
        BuildDashboardPage();
        BuildSetupPage();
        BuildNetworkPage();
        BuildSharingPage();
        BuildMonitorPage();
        BuildDiagnosticsPage();
        BuildSettingsPage();
        BuildShellFallback();
    }

private:
    void BuildNavigation() {
        int x = NativePageLayoutMetrics::pad;
        const int y = NativePageLayoutMetrics::pad;
        window_.m_navDashboardBtn = factory_.CreateButton(L"Dashboard", BS_PUSHBUTTON, x, y, 130, 30, ID_BTN_NAV_DASHBOARD);
        window_.m_navSetupBtn = factory_.CreateButton(L"Setup", BS_PUSHBUTTON, x + 140, y, 130, 30, ID_BTN_NAV_SETUP);
        window_.m_navNetworkBtn = factory_.CreateButton(L"Network", BS_PUSHBUTTON, x + 280, y, 130, 30, ID_BTN_NAV_NETWORK);
        window_.m_navSharingBtn = factory_.CreateButton(L"Sharing", BS_PUSHBUTTON, x + 420, y, 130, 30, ID_BTN_NAV_SHARING);
        window_.m_navMonitorBtn = factory_.CreateButton(L"Monitor", BS_PUSHBUTTON, x + 560, y, 130, 30, ID_BTN_NAV_MONITOR);
        window_.m_navDiagnosticsBtn = factory_.CreateButton(L"Diagnostics", BS_PUSHBUTTON, x + 700, y, 130, 30, ID_BTN_NAV_DIAGNOSTICS);
        window_.m_navSettingsBtn = factory_.CreateButton(L"Settings", BS_PUSHBUTTON, x + 840, y, 130, 30, ID_BTN_NAV_SETTINGS);
    }

    void BuildDashboardPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int rowH = NativePageLayoutMetrics::rowHeight;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;

        window_.m_dashboardIntro = factory_.CreateStatic(
            L"Dashboard shows readiness first. Detailed knobs stay in Setup.",
            0,
            x,
            y,
            leftW - pad * 2,
            rowH);
        y += 28;

        window_.m_dashboardStatusCard = factory_.CreateEdit(L"",
                                                            ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                                            x,
                                                            y,
                                                            leftW - 210 - pad * 3,
                                                            150);
        window_.m_dashboardPrimaryBtn = factory_.CreateButton(L"Start Sharing", BS_PUSHBUTTON, x + leftW - 180 - pad, y + 4, 170, 34, ID_BTN_DASH_START);
        window_.m_dashboardContinueBtn = factory_.CreateButton(L"Continue Setup", BS_PUSHBUTTON, x + leftW - 180 - pad, y + 48, 170, 34, ID_BTN_DASH_CONTINUE);
        window_.m_dashboardWizardBtn = factory_.CreateButton(L"Open Wizard", BS_PUSHBUTTON, x + leftW - 180 - pad, y + 92, 170, 34, ID_BTN_DASH_WIZARD);
        y += 162;

        constexpr int cardGap = 10;
        const int cardW = (leftW - pad * 2 - cardGap) / 2;
        constexpr int cardH = 112;
        window_.m_dashboardNetworkCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x, y, cardW, cardH);
        window_.m_dashboardServiceCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x + cardW + cardGap, y, cardW, cardH);
        y += cardH + cardGap;
        window_.m_dashboardShareCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x, y, cardW, cardH);
        window_.m_dashboardHealthCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x + cardW + cardGap, y, cardW, cardH);
        y += cardH + 18;

        window_.m_dashboardSuggestionsLabel = factory_.CreateStatic(L"Recent Next Steps:", 0, x, y, 160, rowH);
        y += 24;

        const int suggestionTextW = leftW - 310 - pad * 2;
        constexpr int suggestionBtnW = 92;
        for (int i = 0; i < 4; ++i) {
            const int rowY = y + i * 40;
            const int fixId = ID_BTN_DASH_SUGGESTION_FIX_1 + i * 10;
            const int infoId = ID_BTN_DASH_SUGGESTION_INFO_1 + i * 10;
            const int setupId = ID_BTN_DASH_SUGGESTION_SETUP_1 + i * 10;
            window_.m_dashboardSuggestionText[i] = factory_.CreateStatic(L"", 0, x, rowY + 8, suggestionTextW, 20);
            window_.m_dashboardSuggestionFixBtn[i] = factory_.CreateButton(L"Fix", BS_PUSHBUTTON, x + suggestionTextW + 8, rowY + 2, suggestionBtnW, 28, fixId);
            window_.m_dashboardSuggestionInfoBtn[i] = factory_.CreateButton(L"Info", BS_PUSHBUTTON, x + suggestionTextW + 8 + suggestionBtnW + 6, rowY + 2, suggestionBtnW, 28, infoId);
            window_.m_dashboardSuggestionSetupBtn[i] = factory_.CreateButton(L"Setup", BS_PUSHBUTTON, x + suggestionTextW + 8 + (suggestionBtnW + 6) * 2, rowY + 2, suggestionBtnW, 28, setupId);
        }
    }

    void BuildSetupPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;
        constexpr int rowH = NativePageLayoutMetrics::rowHeight;
        constexpr int btnH = NativePageLayoutMetrics::buttonHeight;
        constexpr int labelW = NativePageLayoutMetrics::labelWidth;

        window_.m_setupTitle = factory_.CreateStatic(L"Session Setup", 0, x, y, leftW - pad * 2, 24);
        y += 28;
        window_.m_stepInfo = factory_.CreateEdit(
            L"Use this page only for the current session: room, token, bind, port, start, and host preview.",
            ES_MULTILINE | ES_READONLY | WS_VSCROLL,
            x,
            y,
            leftW - pad * 2,
            42);
        y += 54;

        window_.m_sessionGroup = factory_.CreateButton(L"Module 1: Session Basics", BS_GROUPBOX, x, y, leftW - pad * 2, 108);
        y += 24;

        window_.m_ipLabel = factory_.CreateStatic(L"Host IPv4:", 0, x, y, labelW, rowH);
        window_.m_ipValue = factory_.CreateStatic(window_.m_hostIp, 0, x + labelW, y, 160, rowH);
        window_.m_templateLabel = factory_.CreateStatic(L"Template:", 0, x + 270, y, 70, rowH);
        window_.m_templateCombo = factory_.CreateCombo(CBS_DROPDOWNLIST | WS_VSCROLL, x + 345, y - 3, 190, 200, ID_COMBO_SESSION_TEMPLATE);
        factory_.PopulateCombo(window_.m_templateCombo, {L"Quick Share", L"Fixed Room", L"Demo Mode"}, 0);
        window_.m_btnRefreshIp = factory_.CreateButton(L"Refresh IP", BS_PUSHBUTTON, x + leftW - 130 - pad, y - 1, 120, btnH, ID_BTN_REFRESH_IP);
        y += 30;

        window_.m_roomLabel = factory_.CreateStatic(L"Room:", 0, x, y, labelW, rowH);
        window_.m_roomEdit = factory_.CreateEdit(window_.m_room, ES_AUTOHSCROLL, x + labelW, y - 2, 190, rowH + 4, ID_EDIT_SESSION_ROOM);
        window_.m_tokenLabel = factory_.CreateStatic(L"Token:", 0, x + 300, y, 50, rowH);
        window_.m_tokenEdit = factory_.CreateEdit(window_.m_token, ES_AUTOHSCROLL, x + 355, y - 2, 180, rowH + 4, ID_EDIT_SESSION_TOKEN);
        window_.m_btnGenerate = factory_.CreateButton(L"Auto Generate", BS_PUSHBUTTON, x + leftW - 130 - pad, y - 1, 120, btnH, ID_BTN_GENERATE);
        y += 42;

        window_.m_serviceGroup = factory_.CreateButton(L"Module 2: Service Listen", BS_GROUPBOX, x, y, leftW - pad * 2, 100);
        y += 24;

        window_.m_bindLabel = factory_.CreateStatic(L"Bind:", 0, x, y, labelW, rowH);
        window_.m_bindEdit = factory_.CreateEdit(window_.m_bindAddress, ES_AUTOHSCROLL, x + labelW, y - 2, 190, rowH + 4, ID_EDIT_SESSION_BIND);
        window_.m_portLabel = factory_.CreateStatic(L"Port:", 0, x + 300, y, 50, rowH);
        const std::wstring portText = std::to_wstring(window_.m_port);
        window_.m_portEdit = factory_.CreateEdit(portText, ES_AUTOHSCROLL | ES_NUMBER, x + 355, y - 2, 120, rowH + 4, ID_EDIT_SESSION_PORT);
        window_.m_advancedToggle = factory_.CreateButton(L"Advanced", BS_PUSHBUTTON, x + leftW - 130 - pad, y - 1, 120, btnH, ID_BTN_ADVANCED);
        y += 30;

        window_.m_sanIpLabel = factory_.CreateStatic(L"Host IP:", 0, x, y, 120, rowH);
        window_.m_sanIpValue = factory_.CreateStatic(L"", 0, x + 125, y, leftW - 140 - pad * 2, rowH);
        y += 30;

        window_.m_netCapsText = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 52);
        y += 64;

        window_.m_hotspotLabel = factory_.CreateButton(L"Module 3: Run Control", BS_GROUPBOX, x, y, leftW - pad * 2, 118);
        y += 24;

        window_.m_btnStart = factory_.CreateButton(L"Start", BS_PUSHBUTTON, x, y, 90, btnH, ID_BTN_START);
        window_.m_btnStop = factory_.CreateButton(L"Stop", BS_PUSHBUTTON, x + 100, y, 90, btnH, ID_BTN_STOP);
        window_.m_btnRestart = factory_.CreateButton(L"Restart", BS_PUSHBUTTON, x + 200, y, 90, btnH, ID_BTN_RESTART);
        window_.m_btnServiceOnly = factory_.CreateButton(L"Service Only", BS_PUSHBUTTON, x + 300, y, 110, btnH, ID_BTN_SERVICE_ONLY);
        window_.m_btnStartAndOpenHost = factory_.CreateButton(L"Start + Open Host", BS_PUSHBUTTON, x + 420, y, 160, btnH, ID_BTN_START_AND_OPEN_HOST);
        y += 36;

        window_.m_statusText = factory_.CreateStatic(L"Status: idle", 0, x, y, leftW - pad * 2, rowH);
        y += 22;
        window_.m_statsText = factory_.CreateStatic(L"Stats: Rooms 0 | Viewers 0", 0, x, y, leftW - pad * 2, rowH);
        y += 22;
        window_.m_webStateText = factory_.CreateStatic(L"Host Page: idle | WebView: not ready | Hotspot: stopped", 0, x, y, leftW - pad * 2, rowH);
        y += 34;

        window_.m_shareInfoLabel = factory_.CreateButton(L"Module 4: Session Summary", BS_GROUPBOX, x, y, leftW - pad * 2, 270);
        y += 20;
        window_.m_sessionSummaryLabel = factory_.CreateStatic(L"Live session summary before launch:", 0, x, y, 240, rowH);
        y += 20;
        window_.m_sessionSummaryBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 72);
        y += 82;

        window_.m_firstActionsLabel = factory_.CreateStatic(L"First actions:", 0, x, y, 140, rowH);
        y += 18;
        window_.m_firstActionsBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x, y, leftW - pad * 2, 52);
        y += 60;

        window_.m_shareInfoBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 108);
        y += 96;

        window_.m_btnOpenViewer = factory_.CreateButton(L"Open Viewer", BS_PUSHBUTTON, x, y, 100, btnH, ID_BTN_OPEN_VIEWER);
        window_.m_btnCopyViewer = factory_.CreateButton(L"Copy Viewer", BS_PUSHBUTTON, x + 110, y, 110, btnH, ID_BTN_COPY_VIEWER_URL);
        window_.m_btnShowQr = factory_.CreateButton(L"Show QR", BS_PUSHBUTTON, x + 230, y, 100, btnH, ID_BTN_SHOW_QR);
        window_.m_btnShowWizard = factory_.CreateButton(L"Open Wizard", BS_PUSHBUTTON, x + 340, y, 120, btnH, ID_BTN_OPEN_SHARE_WIZARD);
        window_.m_btnExportBundle = factory_.CreateButton(L"Export Bundle", BS_PUSHBUTTON, x + 470, y, 120, btnH, ID_BTN_EXPORT_BUNDLE);
        y += 36;
        window_.m_btnRunSelfCheck = factory_.CreateButton(L"Run Self-Check", BS_PUSHBUTTON, x, y, 120, btnH, ID_BTN_RUN_SELF_CHECK);
        window_.m_btnRefreshChecks = factory_.CreateButton(L"Refresh Checks", BS_PUSHBUTTON, x + 130, y, 120, btnH, ID_BTN_REFRESH_CHECKS);
        window_.m_btnOpenDiagnostics = factory_.CreateButton(L"Open Diagnostics", BS_PUSHBUTTON, x + 260, y, 140, btnH, ID_BTN_OPEN_DIAGNOSTICS);
        window_.m_btnOpenFolder = factory_.CreateButton(L"Open Output", BS_PUSHBUTTON, x + 410, y, 120, btnH, ID_BTN_OPEN_OUTPUT_FOLDER);
        y += 40;

        window_.m_diagSummaryLabel = factory_.CreateStatic(L"Diagnostics summary:", 0, x, y, 170, rowH);
        y += 18;
        window_.m_diagSummaryBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 92);
        y += 102;

        window_.m_logLabel = factory_.CreateStatic(L"Recent logs:", 0, x, y, 140, rowH);
        y += 18;
        window_.m_logBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 150);

        const int rightX = leftW + pad * 2;
        window_.m_hostPreviewPlaceholder = factory_.CreateEdit(L"",
                                                               ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                                                               rightX,
                                                               pad,
                                                               1,
                                                               1);
        window_.m_btnOpenHost = factory_.CreateButton(L"Open Host In Browser", BS_PUSHBUTTON, rightX, pad, 1, 1, ID_BTN_OPEN_HOST);
        window_.m_runtimeInfoCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, rightX, pad, 1, 1);
    }

    void BuildNetworkPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;
        constexpr int rowH = NativePageLayoutMetrics::rowHeight;
        constexpr int labelW = NativePageLayoutMetrics::labelWidth;
        constexpr int btnH = NativePageLayoutMetrics::buttonHeight;

        window_.m_networkTitle = factory_.CreateStatic(L"Network & Connectivity", 0, x, y, leftW - pad * 2, 24);
        y += 28;
        window_.m_networkSummaryCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - 250 - pad * 3, 110);
        window_.m_btnRefreshNetwork = factory_.CreateButton(L"Re-Detect", BS_PUSHBUTTON, x + leftW - 220 - pad, y + 6, 210, 30, ID_BTN_REFRESH_NETWORK);
        window_.m_btnManualSelectIp = factory_.CreateButton(L"Manual Select Main IP", BS_PUSHBUTTON, x + leftW - 220 - pad, y + 46, 210, 30, ID_BTN_MANUAL_SELECT_IP);
        y += 122;

        window_.m_adapterListLabel = factory_.CreateStatic(L"Adapter Candidates", 0, x, y, 200, rowH);
        y += 24;
        for (int i = 0; i < 4; ++i) {
            const int rowY = y + i * 58;
            window_.m_networkAdapterCards[i] = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x, rowY, leftW - 170 - pad * 3, 48);
            window_.m_networkAdapterSelectBtns[i] = factory_.CreateButton(L"Use As Main", BS_PUSHBUTTON, x + leftW - 160 - pad, rowY + 9, 150, 28, ID_BTN_SELECT_ADAPTER_1 + i);
        }
        y += 4 * 58 + 8;

        window_.m_hotspotGroup = factory_.CreateButton(L"Hotspot", BS_GROUPBOX, x, y, leftW - pad * 2, 148);
        y += 24;
        window_.m_hotspotStatusCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x + 360, y - 4, leftW - 370 - pad * 2, 86);
        window_.m_hotspotSsidLabel = factory_.CreateStatic(L"SSID:", 0, x, y, labelW, rowH);
        window_.m_hotspotSsidEdit = factory_.CreateEdit(window_.m_hotspotSsid, ES_AUTOHSCROLL, x + labelW, y - 2, 230, rowH + 4);
        y += 30;
        window_.m_hotspotPwdLabel = factory_.CreateStatic(L"Password:", 0, x, y, labelW, rowH);
        window_.m_hotspotPwdEdit = factory_.CreateEdit(window_.m_hotspotPassword, ES_AUTOHSCROLL, x + labelW, y - 2, 230, rowH + 4);
        y += 34;
        window_.m_btnAutoHotspot = factory_.CreateButton(L"Auto Generate", BS_PUSHBUTTON, x, y, 120, btnH, ID_BTN_AUTO_HOTSPOT);
        window_.m_btnStartHotspot = factory_.CreateButton(L"Start Hotspot", BS_PUSHBUTTON, x + 130, y, 120, btnH, ID_BTN_START_HOTSPOT);
        window_.m_btnStopHotspot = factory_.CreateButton(L"Stop Hotspot", BS_PUSHBUTTON, x + 260, y, 120, btnH, ID_BTN_STOP_HOTSPOT);
        window_.m_btnOpenHotspotSettings = factory_.CreateButton(L"System Hotspot Settings", BS_PUSHBUTTON, x + 390, y, 180, btnH, ID_BTN_OPEN_HOTSPOT_SETTINGS);
        y += 42;

        window_.m_wifiDirectGroup = factory_.CreateButton(L"Wi-Fi Direct / Pairing", BS_GROUPBOX, x, y, leftW - pad * 2, 120);
        y += 24;
        window_.m_wifiDirectCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x, y, leftW - 210 - pad * 3, 74);
        window_.m_btnPairWifiDirect = factory_.CreateButton(L"Open Connected Devices", BS_PUSHBUTTON, x + leftW - 190 - pad, y, 180, 28, ID_BTN_WIFI_DIRECT_PAIR);
        window_.m_btnOpenConnectedDevices = factory_.CreateButton(L"Open Settings Page", BS_PUSHBUTTON, x + leftW - 190 - pad, y + 34, 180, 28, ID_BTN_OPEN_CONNECTED_DEVICES);
        window_.m_btnOpenPairingHelp = factory_.CreateButton(L"Pairing Help", BS_PUSHBUTTON, x + leftW - 190 - pad, y + 68, 180, 28, ID_BTN_OPEN_PAIRING_HELP);
    }

    void BuildSharingPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;

        window_.m_sharingTitle = factory_.CreateStatic(L"Sharing Center", 0, x, y, leftW - pad * 2, 24);
        y += 28;
        window_.m_accessEntryCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 164);
        window_.m_btnCopyHostUrl = factory_.CreateButton(L"Copy Host URL", BS_PUSHBUTTON, x + 14, y + 112, 130, 28, ID_BTN_COPY_HOST_URL);
        window_.m_btnCopyViewerUrl = factory_.CreateButton(L"Copy Viewer URL", BS_PUSHBUTTON, x + 154, y + 112, 140, 28, ID_BTN_COPY_VIEWER_URL_2);
        window_.m_btnOpenHostBrowser = factory_.CreateButton(L"Open Host In Browser", BS_PUSHBUTTON, x + 304, y + 112, 160, 28, ID_BTN_OPEN_HOST_BROWSER);
        window_.m_btnOpenViewerBrowser = factory_.CreateButton(L"Open Viewer In Browser", BS_PUSHBUTTON, x + 474, y + 112, 170, 28, ID_BTN_OPEN_VIEWER_BROWSER);
        y += 176;

        window_.m_qrCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 176);
        window_.m_btnSaveQrImage = factory_.CreateButton(L"Save QR Image", BS_PUSHBUTTON, x + 14, y + 124, 120, 28, ID_BTN_SAVE_QR_IMAGE);
        window_.m_btnFullscreenQr = factory_.CreateButton(L"Fullscreen QR", BS_PUSHBUTTON, x + 144, y + 124, 120, 28, ID_BTN_FULLSCREEN_QR);
        window_.m_btnOpenShareCard = factory_.CreateButton(L"Open Share Card", BS_PUSHBUTTON, x + 274, y + 124, 130, 28, ID_BTN_OPEN_SHARE_CARD_2);
        window_.m_btnOpenShareWizard = factory_.CreateButton(L"Open Share Wizard", BS_PUSHBUTTON, x + 414, y + 124, 140, 28, ID_BTN_OPEN_SHARE_WIZARD_2);
        window_.m_btnOpenBundleFolder = factory_.CreateButton(L"Open Bundle Folder", BS_PUSHBUTTON, x + 564, y + 124, 140, 28, ID_BTN_OPEN_BUNDLE_FOLDER_2);
        y += 188;

        window_.m_accessGuideCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 190);
        window_.m_btnExportOfflineZip = factory_.CreateButton(L"Export Offline Package", BS_PUSHBUTTON, x + 14, y + 148, 180, 28, ID_BTN_EXPORT_OFFLINE_ZIP);
    }

    void BuildMonitorPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;
        window_.m_monitorTitle = factory_.CreateStatic(L"Live Monitor", 0, x, y, leftW - pad * 2, 24);
        y += 30;
        constexpr int monitorGap = 8;
        const int metricW = (leftW - pad * 2 - monitorGap * 4) / 5;
        for (int i = 0; i < 5; ++i) {
            window_.m_monitorMetricCards[i] = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY, x + i * (metricW + monitorGap), y, metricW, 70);
        }
        y += 82;
        window_.m_monitorTimelineLabel = factory_.CreateStatic(L"Session Timeline", 0, x, y, 200, 22);
        y += 24;
        window_.m_monitorTimelineBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 150);
        y += 162;
        window_.m_monitorTabHealth = factory_.CreateButton(L"Health Checks", BS_PUSHBUTTON, x, y, 130, 28);
        window_.m_monitorTabConnections = factory_.CreateButton(L"Connection Events", BS_PUSHBUTTON, x + 140, y, 150, 28);
        window_.m_monitorTabLogs = factory_.CreateButton(L"Realtime Logs", BS_PUSHBUTTON, x + 300, y, 130, 28);
        y += 36;
        window_.m_monitorDetailBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 260);
    }

    void BuildDiagnosticsPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;
        window_.m_diagPageTitle = factory_.CreateStatic(L"Diagnostics & Export", 0, x, y, leftW - pad * 2, 24);
        y += 28;
        window_.m_diagChecklistCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, 230, 180);
        window_.m_diagActionsCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x + 240, y, 230, 180);
        window_.m_diagExportCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x + 480, y, 220, 180);
        y += 192;
        window_.m_diagFilesCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 116);
        y += 128;
        window_.m_diagLogSearch = factory_.CreateEdit(L"", ES_AUTOHSCROLL, x, y, 200, 24, ID_EDIT_DIAG_LOG_SEARCH);
        window_.m_diagLevelFilter = factory_.CreateCombo(CBS_DROPDOWNLIST | WS_VSCROLL, x + 210, y - 2, 120, 160, ID_COMBO_DIAG_LEVEL);
        factory_.PopulateCombo(window_.m_diagLevelFilter, {L"All Levels", L"Info", L"Warning", L"Error"}, 0);
        window_.m_diagSourceFilter = factory_.CreateCombo(CBS_DROPDOWNLIST | WS_VSCROLL, x + 340, y - 2, 140, 160, ID_COMBO_DIAG_SOURCE);
        factory_.PopulateCombo(window_.m_diagSourceFilter, {L"All Sources", L"app", L"network", L"server", L"webview"}, 0);
        window_.m_btnDiagCopyLogs = factory_.CreateButton(L"Copy Logs", BS_PUSHBUTTON, x + 490, y - 1, 100, 26, ID_BTN_DIAG_COPY_LOGS);
        window_.m_btnDiagSaveLogs = factory_.CreateButton(L"Save Logs", BS_PUSHBUTTON, x + 600, y - 1, 100, 26, ID_BTN_DIAG_SAVE_LOGS);
        y += 34;
        window_.m_diagLogViewer = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 224);
        window_.m_btnDiagOpenOutput = factory_.CreateButton(L"Open Output", BS_PUSHBUTTON, x + 490, NativePageLayoutMetrics::setupTop + 132, 100, 26, ID_BTN_DIAG_OPEN_OUTPUT);
        window_.m_btnDiagOpenReport = factory_.CreateButton(L"Open Report", BS_PUSHBUTTON, x + 600, NativePageLayoutMetrics::setupTop + 132, 100, 26, ID_BTN_DIAG_OPEN_REPORT);
        window_.m_btnDiagExportZip = factory_.CreateButton(L"Export Zip", BS_PUSHBUTTON, x + 490, NativePageLayoutMetrics::setupTop + 162, 100, 26, ID_BTN_DIAG_EXPORT_ZIP);
        window_.m_btnDiagCopyPath = factory_.CreateButton(L"Copy Path", BS_PUSHBUTTON, x + 600, NativePageLayoutMetrics::setupTop + 162, 100, 26, ID_BTN_DIAG_COPY_PATH);
        window_.m_btnDiagRefreshBundle = factory_.CreateButton(L"Refresh Bundle", BS_PUSHBUTTON, x + 545, NativePageLayoutMetrics::setupTop + 102, 120, 26, ID_BTN_DIAG_REFRESH_BUNDLE);
    }

    void BuildSettingsPage() {
        const int x = NativePageLayoutMetrics::pad;
        int y = NativePageLayoutMetrics::setupTop;
        constexpr int leftW = NativePageLayoutMetrics::leftPaneWidth;
        constexpr int pad = NativePageLayoutMetrics::pad;
        window_.m_settingsTitle = factory_.CreateStatic(L"Settings", 0, x, y, leftW - pad * 2, 24);
        y += 28;
        window_.m_settingsIntro = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 56);
        y += 68;
        window_.m_settingsGeneralCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, 226, 176);
        window_.m_settingsServiceCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x + 236, y, 226, 176);
        window_.m_settingsNetworkCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x + 472, y, 228, 176);
        y += 188;
        window_.m_settingsSharingCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, 226, 176);
        window_.m_settingsLoggingCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x + 236, y, 226, 176);
        window_.m_settingsAdvancedCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x + 472, y, 228, 176);
        y += 188;
        window_.m_settingsCurrentStateCard = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, x, y, leftW - pad * 2, 170);
    }

    void BuildShellFallback() {
        window_.m_shellFallbackBox = factory_.CreateEdit(L"", ES_MULTILINE | ES_READONLY | WS_VSCROLL, 0, 0, 1, 1);
        window_.m_shellRetryBtn = factory_.CreateButton(L"Retry Loading UI", BS_PUSHBUTTON, 0, 0, 1, 1, ID_BTN_SHELL_RETRY);
        window_.m_shellStartBtn = factory_.CreateButton(L"Start Service", BS_PUSHBUTTON, 0, 0, 1, 1, ID_BTN_START);
        window_.m_shellStartHostBtn = factory_.CreateButton(L"Start + Open Host", BS_PUSHBUTTON, 0, 0, 1, 1, ID_BTN_START_AND_OPEN_HOST);
        window_.m_shellOpenHostBtn = factory_.CreateButton(L"Open Host In Browser", BS_PUSHBUTTON, 0, 0, 1, 1, ID_BTN_SHELL_OPEN_HOST);
    }

    MainWindow& window_;
    NativeControlFactory factory_;
};

void DesktopHostPageBuilders::BuildAll(MainWindow& window) {
    DesktopHostPageBuilderImpl(window).BuildAll();
}
