#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class AdminBackend {
public:
    struct AdapterCandidate {
        std::wstring name;
        std::wstring ip;
        std::wstring type;
        bool recommended = false;
        bool selected = false;
    };

    struct Snapshot {
        std::wstring appName;
        std::wstring nativePage;
        std::wstring dashboardState;
        std::wstring dashboardLabel;
        std::wstring dashboardError;
        bool canStartSharing = false;
        bool sharingActive = false;
        bool serverRunning = false;
        bool healthReady = false;
        bool hostReachable = false;
        bool certReady = false;
        std::wstring certDetail;
        std::wstring certExpectedSans;
        bool wifiAdapterPresent = false;
        bool hotspotSupported = false;
        bool hotspotRunning = false;
        bool wifiDirectAvailable = false;
        int activeIpv4Candidates = 0;
        int port = 0;
        std::size_t rooms = 0;
        std::size_t viewers = 0;
        std::wstring hostIp;
        std::wstring bind;
        std::wstring room;
        std::wstring token;
        std::wstring hostUrl;
        std::wstring viewerUrl;
        std::wstring networkMode;
        std::wstring hostState;
        std::wstring hotspotStatus;
        std::wstring hotspotSsid;
        std::wstring hotspotPassword;
        std::wstring webviewStatus;
        std::wstring recentHeartbeat;
        std::wstring localReachability;
        std::wstring outputDir;
        std::wstring bundleDir;
        std::wstring serverExePath;
        std::wstring certDir;
        std::wstring timelineText;
        std::wstring logTail;
        bool viewerUrlCopied = false;
        bool shareBundleExported = false;
        std::wstring lastError;
        std::vector<AdapterCandidate> networkCandidates;

        int defaultPort = 9443;
        std::wstring defaultBind;
        std::wstring roomRule;
        std::wstring tokenRule;
        std::wstring logLevel;
        std::wstring defaultViewerOpenMode;
        bool autoCopyViewerLink = false;
        bool autoGenerateQr = false;
        bool autoExportBundle = false;
        bool saveStdStreams = false;
        std::wstring certBypassPolicy;
        std::wstring webViewBehavior;
        std::wstring startupHook;
    };

    struct Handlers {
        std::function<void()> refreshNetwork;
        std::function<void()> generateRoomToken;
        std::function<void()> startServer;
        std::function<void()> stopServer;
        std::function<void()> startServiceOnly;
        std::function<void()> startAndOpenHost;
        std::function<void()> openHost;
        std::function<void()> openViewer;
        std::function<void()> copyHostUrl;
        std::function<void()> copyViewerUrl;
        std::function<void()> exportBundle;
        std::function<void()> openOutput;
        std::function<void()> openReport;
        std::function<void()> refreshBundle;
        std::function<void()> showShareWizard;
        std::function<void(std::size_t index)> selectNetworkCandidate;
        std::function<void()> startHotspot;
        std::function<void()> stopHotspot;
        std::function<void()> autoHotspot;
        std::function<void()> openHotspotSettings;
        std::function<void()> openConnectedDevices;
        std::function<void(std::wstring page)> navigatePage;
        std::function<void(std::wstring room, std::wstring token, std::wstring bind, int port)> applySessionConfig;
        std::function<void(std::wstring ssid, std::wstring password)> applyHotspotConfig;
    };

    struct HandleResult {
        bool requestSnapshot = false;
        bool stateChanged = false;
        std::wstring logLine;
    };

    void SetHandlers(Handlers handlers);
    HandleResult HandleMessage(std::wstring_view payload) const;
    std::wstring BuildSnapshotEventJson(const Snapshot& snapshot) const;

private:
    Handlers m_handlers;
};
