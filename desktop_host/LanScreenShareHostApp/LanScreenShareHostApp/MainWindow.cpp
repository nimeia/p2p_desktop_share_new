#include "pch.h"
#include "MainWindow.h"

#include "UrlUtil.h"
#include "HttpClient.h"
#include "../../../src/core/network/network_manager.h"

#define WIN32_LEAN_AND_MEAN
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

const wchar_t CLASS_NAME[] = L"LanScreenShareHostApp";
const UINT_PTR TIMER_ID = 1;
const UINT WM_APP_LOG = WM_APP + 1;
const UINT WM_APP_POLL = WM_APP + 2;
const UINT WM_APP_WEBVIEW = WM_APP + 3;

// Control IDs
enum {
    ID_BTN_REFRESH_IP = 1001,
    ID_BTN_GENERATE = 1002,
    ID_BTN_START = 1003,
    ID_BTN_STOP = 1004,
    ID_BTN_OPEN_HOST = 1005,
    ID_BTN_OPEN_VIEWER = 1006,
    ID_BTN_COPY_VIEWER = 1007,
    ID_BTN_SHOW_QR = 1008,
    ID_BTN_OPEN_FOLDER = 1009,
    ID_BTN_START_HOTSPOT = 1010,
    ID_BTN_STOP_HOTSPOT = 1011,
    ID_BTN_WIFI_DIRECT_PAIR = 1012,
    ID_BTN_OPEN_HOTSPOT_SETTINGS = 1013,
    ID_BTN_SHOW_WIZARD = 1014,
    ID_BTN_EXPORT_BUNDLE = 1015,
    ID_BTN_RUN_SELF_CHECK = 1016,
    ID_BTN_OPEN_DIAGNOSTICS = 1017,
    ID_BTN_REFRESH_CHECKS = 1018,
    ID_BTN_NAV_DASHBOARD = 1019,
    ID_BTN_NAV_SETUP = 1020,
    ID_BTN_DASH_START = 1021,
    ID_BTN_DASH_CONTINUE = 1022,
    ID_BTN_DASH_WIZARD = 1023,
    ID_BTN_DASH_SUGGESTION_FIX_1 = 1101,
    ID_BTN_DASH_SUGGESTION_INFO_1 = 1102,
    ID_BTN_DASH_SUGGESTION_SETUP_1 = 1103,
    ID_BTN_DASH_SUGGESTION_FIX_2 = 1111,
    ID_BTN_DASH_SUGGESTION_INFO_2 = 1112,
    ID_BTN_DASH_SUGGESTION_SETUP_2 = 1113,
    ID_BTN_DASH_SUGGESTION_FIX_3 = 1121,
    ID_BTN_DASH_SUGGESTION_INFO_3 = 1122,
    ID_BTN_DASH_SUGGESTION_SETUP_3 = 1123,
    ID_BTN_DASH_SUGGESTION_FIX_4 = 1131,
    ID_BTN_DASH_SUGGESTION_INFO_4 = 1132,
    ID_BTN_DASH_SUGGESTION_SETUP_4 = 1133,
    ID_COMBO_SESSION_TEMPLATE = 1140,
    ID_BTN_RESTART = 1141,
    ID_BTN_SERVICE_ONLY = 1142,
    ID_BTN_START_AND_OPEN_HOST = 1143,
    ID_BTN_ADVANCED = 1144,
    ID_BTN_NAV_NETWORK = 1145,
    ID_BTN_REFRESH_NETWORK = 1146,
    ID_BTN_MANUAL_SELECT_IP = 1147,
    ID_BTN_AUTO_HOTSPOT = 1148,
    ID_BTN_OPEN_CONNECTED_DEVICES = 1149,
    ID_BTN_OPEN_PAIRING_HELP = 1150,
    ID_BTN_SELECT_ADAPTER_1 = 1161,
    ID_BTN_SELECT_ADAPTER_2 = 1162,
    ID_BTN_SELECT_ADAPTER_3 = 1163,
    ID_BTN_SELECT_ADAPTER_4 = 1164,
    ID_BTN_NAV_SHARING = 1165,
    ID_BTN_COPY_HOST_URL = 1166,
    ID_BTN_COPY_VIEWER_URL_2 = 1167,
    ID_BTN_OPEN_HOST_BROWSER = 1168,
    ID_BTN_OPEN_VIEWER_BROWSER = 1169,
    ID_BTN_SAVE_QR_IMAGE = 1170,
    ID_BTN_FULLSCREEN_QR = 1171,
    ID_BTN_OPEN_SHARE_CARD_2 = 1172,
    ID_BTN_OPEN_SHARE_WIZARD_2 = 1173,
    ID_BTN_OPEN_BUNDLE_FOLDER_2 = 1174,
    ID_BTN_EXPORT_OFFLINE_ZIP = 1175,
    ID_BTN_NAV_MONITOR = 1176,
    ID_BTN_NAV_DIAGNOSTICS = 1177,
    ID_EDIT_DIAG_LOG_SEARCH = 1178,
    ID_COMBO_DIAG_LEVEL = 1179,
    ID_COMBO_DIAG_SOURCE = 1180,
    ID_BTN_DIAG_OPEN_OUTPUT = 1181,
    ID_BTN_DIAG_OPEN_REPORT = 1182,
    ID_BTN_DIAG_EXPORT_ZIP = 1183,
    ID_BTN_DIAG_COPY_PATH = 1184,
    ID_BTN_DIAG_REFRESH_BUNDLE = 1185,
    ID_BTN_DIAG_COPY_LOGS = 1186,
    ID_BTN_DIAG_SAVE_LOGS = 1187,
    ID_BTN_NAV_SETTINGS = 1188,
};

struct PollResult {
    DWORD status = 0;
    size_t rooms = 0;
    size_t viewers = 0;
};

struct AdapterCandidate {
    std::wstring name;
    std::wstring ip;
    int score = 0;
};

static std::wstring NowTs() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    tm t{};
    localtime_s(&t, &tt);
    wchar_t buf[32];
    wcsftime(buf, _countof(buf), L"%H:%M:%S", &t);
    return buf;
}

static std::wstring NowDateTime() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    tm t{};
    localtime_s(&t, &tt);
    wchar_t buf[64];
    wcsftime(buf, _countof(buf), L"%Y-%m-%d %H:%M:%S", &t);
    return buf;
}

static std::wstring DetectBestIPv4() {
    ULONG bufLen = 15 * 1024;
    std::vector<unsigned char> buf(bufLen);

    IP_ADAPTER_ADDRESSES* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufLen);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen);
    }
    if (ret != NO_ERROR) {
        return L"";
    }

    auto isPrivate = [](uint32_t ipHostOrder) {
        if ((ipHostOrder & 0xFF000000u) == 0x0A000000u) return true;
        if ((ipHostOrder & 0xFFF00000u) == 0xAC100000u) return true;
        if ((ipHostOrder & 0xFFFF0000u) == 0xC0A80000u) return true;
        return false;
    };

    std::wstring best;
    for (auto* a = addrs; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr) continue;
            if (u->Address.lpSockaddr->sa_family != AF_INET) continue;

            auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            uint32_t ipNet = sa->sin_addr.S_un.S_addr;
            uint32_t ipHost = ntohl(ipNet);
            wchar_t ipStr[32]{};
            InetNtopW(AF_INET, &sa->sin_addr, ipStr, _countof(ipStr));

            std::wstring candidate = ipStr;
            if (candidate.empty() || candidate == L"0.0.0.0") continue;

            if (isPrivate(ipHost)) {
                return candidate;
            }

            if (best.empty()) best = candidate;
        }
    }

    return best;
}

static bool IsPrivateIpv4HostOrder(uint32_t ipHostOrder) {
    if ((ipHostOrder & 0xFF000000u) == 0x0A000000u) return true;
    if ((ipHostOrder & 0xFFF00000u) == 0xAC100000u) return true;
    if ((ipHostOrder & 0xFFFF0000u) == 0xC0A80000u) return true;
    return false;
}

static bool LooksVirtualAdapterName(std::wstring_view name) {
    std::wstring lowered(name);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(::towlower(c));
    });
    return lowered.find(L"virtual") != std::wstring::npos ||
           lowered.find(L"hyper-v") != std::wstring::npos ||
           lowered.find(L"vmware") != std::wstring::npos ||
           lowered.find(L"vethernet") != std::wstring::npos ||
           lowered.find(L"virtualbox") != std::wstring::npos ||
           lowered.find(L"wsl") != std::wstring::npos ||
           lowered.find(L"vpn") != std::wstring::npos ||
           lowered.find(L"tap") != std::wstring::npos ||
           lowered.find(L"tun") != std::wstring::npos;
}

static std::vector<AdapterCandidate> CollectActiveIpv4Candidates() {
    ULONG bufLen = 15 * 1024;
    std::vector<unsigned char> buf(bufLen);

    IP_ADAPTER_ADDRESSES* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;

    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf.resize(bufLen);
        addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen);
    }
    if (ret != NO_ERROR) {
        return {};
    }

    std::vector<AdapterCandidate> out;
    for (auto* a = addrs; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp) continue;
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK || a->IfType == IF_TYPE_TUNNEL) continue;

        const std::wstring friendly = a->FriendlyName ? a->FriendlyName : L"adapter";
        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;

            auto* sa = reinterpret_cast<sockaddr_in*>(u->Address.lpSockaddr);
            uint32_t ipHost = ntohl(sa->sin_addr.S_un.S_addr);
            wchar_t ipStr[32]{};
            InetNtopW(AF_INET, &sa->sin_addr, ipStr, _countof(ipStr));
            std::wstring candidateIp = ipStr;
            if (candidateIp.empty() || candidateIp == L"0.0.0.0" || candidateIp.rfind(L"169.254.", 0) == 0) continue;

            AdapterCandidate candidate;
            candidate.name = friendly;
            candidate.ip = candidateIp;
            candidate.score = IsPrivateIpv4HostOrder(ipHost) ? 200 : 20;
            if (a->IfType == IF_TYPE_IEEE80211) candidate.score += 40;
            if (a->FirstGatewayAddress && a->FirstGatewayAddress->Address.lpSockaddr) candidate.score += 20;
            if (LooksVirtualAdapterName(friendly)) candidate.score -= 120;
            out.push_back(std::move(candidate));
        }
    }

    std::sort(out.begin(), out.end(), [](const AdapterCandidate& a, const AdapterCandidate& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.ip != b.ip) return a.ip < b.ip;
        return a.name < b.name;
    });

    std::vector<AdapterCandidate> unique;
    for (const auto& item : out) {
        const bool exists = std::any_of(unique.begin(), unique.end(), [&](const AdapterCandidate& v) {
            return v.ip == item.ip;
        });
        if (!exists) unique.push_back(item);
    }
    return unique;
}

static bool ParseApiStatus(const std::string& body, size_t& rooms, size_t& viewers) {
    auto findNum = [&](const char* key, size_t& out) -> bool {
        std::string k = std::string("\"") + key + "\":";
        auto pos = body.find(k);
        if (pos == std::string::npos) return false;
        pos += k.size();
        while (pos < body.size() && body[pos] == ' ') pos++;
        size_t end = pos;
        while (end < body.size() && (body[end] >= '0' && body[end] <= '9')) end++;
        if (end == pos) return false;
        out = static_cast<size_t>(std::stoull(body.substr(pos, end - pos)));
        return true;
    };

    size_t r = 0, v = 0;
    bool ok1 = findNum("rooms", r);
    bool ok2 = findNum("viewers", v);
    if (!ok1 || !ok2) return false;
    rooms = r;
    viewers = v;
    return true;
}

static std::wstring JsonStringField(std::wstring_view body, std::wstring_view key) {
    std::wstring pattern = L"\"" + std::wstring(key) + L"\":\"";
    const size_t start = body.find(pattern);
    if (start == std::wstring_view::npos) return L"";

    std::wstring out;
    bool escaped = false;
    for (size_t i = start + pattern.size(); i < body.size(); ++i) {
        wchar_t c = body[i];
        if (escaped) {
            out.push_back(c);
            escaped = false;
            continue;
        }
        if (c == L'\\') {
            escaped = true;
            continue;
        }
        if (c == L'"') break;
        out.push_back(c);
    }
    return out;
}

static bool JsonIntField(std::wstring_view body, std::wstring_view key, std::size_t& value) {
    std::wstring pattern = L"\"" + std::wstring(key) + L"\":";
    const size_t start = body.find(pattern);
    if (start == std::wstring_view::npos) return false;

    size_t pos = start + pattern.size();
    while (pos < body.size() && body[pos] == L' ') ++pos;
    size_t end = pos;
    while (end < body.size() && body[end] >= L'0' && body[end] <= L'9') ++end;
    if (end == pos) return false;
    value = static_cast<std::size_t>(std::stoull(std::wstring(body.substr(pos, end - pos))));
    return true;
}

static std::wstring HtmlEscape(std::wstring_view value) {
    std::wstring out;
    out.reserve(value.size() + 16);
    for (wchar_t c : value) {
        switch (c) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'\"': out += L"&quot;"; break;
        case L'\'': out += L"&#39;"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

struct CertArtifactsInfo {
    fs::path certDir;
    fs::path certFile;
    fs::path keyFile;
    bool certExists = false;
    bool keyExists = false;
};

struct RuntimeDiagnosticsSnapshot {
    bool serverProcessRunning = false;
    bool portReady = false;
    std::wstring portDetail;
    bool localHealthReady = false;
    std::wstring localHealthDetail;
    bool hostIpReachable = false;
    std::wstring hostIpReachableDetail;
    bool lanBindReady = false;
    std::wstring lanBindDetail;
    bool embeddedHostReady = false;
    std::wstring embeddedHostStatus;
    int activeIpv4Candidates = 0;
    bool selectedIpRecommended = true;
    std::wstring adapterHint;
};

static CertArtifactsInfo ProbeCertArtifacts(const fs::path& appDir) {
    CertArtifactsInfo info;
    info.certDir = appDir / L"cert";
    info.certFile = info.certDir / L"server.crt";
    info.keyFile = info.certDir / L"server.key";
    info.certExists = fs::exists(info.certFile);
    info.keyExists = fs::exists(info.keyFile);
    return info;
}

static bool IsHostStateServerRunning(std::wstring_view state) {
    return !state.empty() && state != L"stopped";
}

static bool IsHostStateSharing(std::wstring_view state) {
    return state == L"sharing";
}

static bool IsHostStateReadyOrLoading(std::wstring_view state) {
    return state == L"ready" || state == L"loading" || state == L"sharing";
}

static std::wstring LastSocketErrorText(int err) {
    wchar_t buf[64];
    _snwprintf_s(buf, _TRUNCATE, L"WSA %d", err);
    return buf;
}

static bool CanBindTcpPort(std::wstring_view bindAddress, int port, std::wstring* detail) {
    if (port <= 0 || port > 65535) {
        if (detail) *detail = L"Port is outside the valid TCP range.";
        return false;
    }

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        if (detail) *detail = L"WSAStartup failed.";
        return false;
    }

    bool ok = false;
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        if (detail) *detail = L"socket() failed: " + LastSocketErrorText(WSAGetLastError());
        WSACleanup();
        return false;
    }

    BOOL exclusive = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));

    std::wstring probeBind = bindAddress.empty() ? L"0.0.0.0" : std::wstring(bindAddress);
    if (probeBind == L"*" || probeBind == L"0.0.0.0" || probeBind == L"localhost") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        probeBind = L"0.0.0.0";
    } else if (InetPtonW(AF_INET, probeBind.c_str(), &addr.sin_addr) != 1) {
        if (detail) *detail = L"Bind address is not a supported IPv4 address: " + probeBind;
        closesocket(sock);
        WSACleanup();
        return false;
    }

    if (bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0) {
        ok = true;
        if (detail) *detail = L"Port " + std::to_wstring(port) + L" is free for bind " + probeBind + L".";
    } else {
        if (detail) {
            *detail = L"Port " + std::to_wstring(port) + L" cannot bind on " + probeBind +
                      L": " + LastSocketErrorText(WSAGetLastError());
        }
    }

    closesocket(sock);
    WSACleanup();
    return ok;
}

static RuntimeDiagnosticsSnapshot CollectRuntimeDiagnostics(const ServerController* server,
                                                           const WebViewHost& webview,
                                                           std::wstring_view bindAddress,
                                                           std::wstring_view hostIp,
                                                           int port) {
    RuntimeDiagnosticsSnapshot snapshot;
    snapshot.serverProcessRunning = server && server->IsRunning();
    snapshot.embeddedHostReady = webview.IsReady();
    snapshot.embeddedHostStatus = webview.StatusText();

    const auto candidates = CollectActiveIpv4Candidates();
    snapshot.activeIpv4Candidates = static_cast<int>(candidates.size());
    if (candidates.empty()) {
        snapshot.adapterHint = L"No active IPv4 LAN candidate was detected.";
    } else if (candidates.size() == 1) {
        snapshot.adapterHint = L"Single active IPv4 candidate: " + candidates.front().ip + L" on " + candidates.front().name + L".";
    } else {
        snapshot.adapterHint = L"Multiple active IPv4 candidates detected. Recommended: " + candidates.front().ip +
                               L" on " + candidates.front().name + L".";
    }
    if (!hostIp.empty() && !candidates.empty()) {
        snapshot.selectedIpRecommended = candidates.front().ip == hostIp;
        if (!snapshot.selectedIpRecommended) {
            snapshot.adapterHint += L" Current selected IP is " + std::wstring(hostIp) + L".";
        }
    }

    if (snapshot.serverProcessRunning) {
        snapshot.portReady = true;
        snapshot.portDetail = L"Port " + std::to_wstring(port) + L" is already owned by the local server process.";
    } else {
        snapshot.portReady = CanBindTcpPort(bindAddress, port, &snapshot.portDetail);
    }

    std::wstring bindValue = bindAddress.empty() ? L"0.0.0.0" : std::wstring(bindAddress);
    if (bindValue == L"localhost") bindValue = L"127.0.0.1";
    if (bindValue == L"*" || bindValue == L"0.0.0.0") {
        snapshot.lanBindReady = true;
        snapshot.lanBindDetail = L"Server bind address allows LAN clients.";
    } else if (bindValue == L"127.0.0.1") {
        snapshot.lanBindReady = false;
        snapshot.lanBindDetail = L"Bind address is loopback-only. Other devices cannot reach this host.";
    } else {
        snapshot.lanBindReady = true;
        snapshot.lanBindDetail = L"Server binds to specific host address " + bindValue + L".";
    }

    if (snapshot.serverProcessRunning) {
        const std::wstring url = L"https://127.0.0.1:" + std::to_wstring(port) + L"/health";
        const HttpResponse health = HttpClient::Get(url, 700);
        snapshot.localHealthReady = health.status == 200 && health.body.find("ok") != std::string::npos;
        if (snapshot.localHealthReady) {
            snapshot.localHealthDetail = L"Local /health responded with HTTP 200.";
        } else if (!health.error.empty()) {
            snapshot.localHealthDetail = L"Local /health probe failed: " + health.error;
        } else if (health.status != 0) {
            snapshot.localHealthDetail = L"Local /health responded with HTTP " + std::to_wstring(health.status) + L".";
        } else {
            snapshot.localHealthDetail = L"Local /health did not return a usable response.";
        }
    } else {
        snapshot.localHealthReady = false;
        snapshot.localHealthDetail = L"Local server process is not running, so /health was not probed.";
    }

    if (snapshot.serverProcessRunning && !hostIp.empty() && hostIp != L"(not found)" && hostIp != L"0.0.0.0") {
        const std::wstring url = L"https://" + std::wstring(hostIp) + L":" + std::to_wstring(port) + L"/health";
        const HttpResponse health = HttpClient::Get(url, 900);
        snapshot.hostIpReachable = health.status == 200 && health.body.find("ok") != std::string::npos;
        if (snapshot.hostIpReachable) {
            snapshot.hostIpReachableDetail = L"Selected host IP responded to /health: " + std::wstring(hostIp) + L".";
        } else if (!health.error.empty()) {
            snapshot.hostIpReachableDetail = L"Selected host IP probe failed: " + health.error;
        } else if (health.status != 0) {
            snapshot.hostIpReachableDetail = L"Selected host IP responded with HTTP " + std::to_wstring(health.status) + L".";
        } else {
            snapshot.hostIpReachableDetail = L"Selected host IP did not return a usable /health response.";
        }
    } else if (hostIp.empty() || hostIp == L"(not found)" || hostIp == L"0.0.0.0") {
        snapshot.hostIpReachable = false;
        snapshot.hostIpReachableDetail = L"Selected host IP is missing, so LAN endpoint reachability was not probed.";
    } else {
        snapshot.hostIpReachable = false;
        snapshot.hostIpReachableDetail = L"Server is not running, so LAN endpoint reachability was not probed.";
    }

    return snapshot;
}

static std::string BuildShareCardHtml(std::wstring_view networkMode,
                                      std::wstring_view hostIp,
                                      int port,
                                      std::wstring_view room,
                                      std::wstring_view token,
                                      std::wstring_view hostState,
                                      std::size_t rooms,
                                      std::size_t viewers,
                                      std::wstring_view hotspotSsid,
                                      std::wstring_view hotspotPassword,
                                      bool hotspotRunning,
                                      bool wifiDirectApiAvailable,
                                      std::wstring_view hostUrl,
                                      std::wstring_view viewerUrl,
                                      std::string_view bundleJson) {
    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>LAN Screen Share Share Card</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { font-family: system-ui, -apple-system, "Segoe UI", sans-serif; background: #0f1115; color: #f2f3f5; margin: 0; }
    .page { max-width: 1180px; margin: 0 auto; padding: 24px; }
    .grid { display: grid; grid-template-columns: minmax(340px, 1fr) 420px; gap: 20px; align-items: start; }
    .card { background: #161a22; border: 1px solid #2a3040; border-radius: 18px; padding: 18px; box-shadow: 0 10px 30px rgba(0,0,0,.22); }
    h1 { margin-top: 0; font-size: 28px; }
    .meta { display: grid; grid-template-columns: 132px 1fr; gap: 8px 12px; margin-bottom: 18px; }
    .meta div:nth-child(odd) { color: #93a1b7; }
    .url, .mono { display: block; overflow-wrap: anywhere; background: #0f1115; border: 1px solid #2a3040; border-radius: 12px; padding: 12px; margin-top: 8px; color: #9ed0ff; text-decoration: none; font-family: ui-monospace, SFMono-Regular, Consolas, monospace; }
    .mono { color: #f2f3f5; }
    .actions { display:flex; gap:12px; flex-wrap:wrap; margin-top:16px; }
    button { padding: 10px 14px; border-radius: 10px; border: 1px solid #334056; background:#1c2330; color:#fff; cursor:pointer; }
    .tip { color:#93a1b7; font-size:13px; line-height:1.6; }
    .badge { display:inline-block; border-radius:999px; padding:6px 10px; font-size:12px; background:#1e293b; color:#dbeafe; border:1px solid #334155; }
    .poster { display:flex; flex-direction:column; gap:16px; }
    .poster-hero { border-radius: 18px; border: 1px solid #2a3040; background: linear-gradient(135deg, #1f2937, #111827); padding: 20px; }
    .poster-title { font-size: 26px; font-weight: 700; margin: 0 0 10px; }
    .poster-url { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; line-height: 1.55; font-size: 16px; background: rgba(15,17,21,.66); border: 1px solid rgba(148,163,184,.25); padding: 14px; border-radius: 14px; overflow-wrap:anywhere; }
    .qr-shell { display:flex; align-items:center; justify-content:center; padding:18px; background:#ffffff; border-radius:18px; min-height:320px; }
    .qr-shell svg { width:min(100%, 320px); height:auto; display:block; }
    .statusbar { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-bottom:16px; }
    .live { color:#67e8f9; }
    .warn { color:#fbbf24; }
    .accent { color:#7dd3fc; }
    ol { margin: 10px 0 0 18px; padding: 0; }
    li { margin: 8px 0; }
    @media (max-width: 960px) { .grid { grid-template-columns: 1fr; } }
  </style>
</head>
<body>
  <main class="page">
    <div class="grid">
      <section class="card">
        <div class="statusbar">
          <span class="badge">Offline share card + local QR SVG</span>
          <span class="badge live">Live status: auto refresh</span>
          <span class="tip">Last sync: <span id="lastSyncText">embedded snapshot</span></span>
        </div>
        <h1>LAN Screen Share</h1>
        <div class="meta">
          <div>Mode</div><div id="modeText"></div>
          <div>Host IPv4</div><div id="hostIpText"></div>
          <div>Port</div><div id="portText"></div>
          <div>Room</div><div id="roomText"></div>
          <div>Token</div><div id="tokenText"></div>
          <div>Host State</div><div id="hostStateText"></div>
          <div>Rooms / Viewers</div><div><span id="roomsText"></span> / <span id="viewersText"></span></div>
          <div>Hotspot</div><div id="hotspotStateText"></div>
          <div>Wi-Fi Direct API</div><div id="wifiDirectApiText"></div>
          <div>Bundle Time</div><div id="generatedAtText"></div>
        </div>

        <div class="tip">Host URL</div>
        <a id="hostUrlLink" class="url" href="#"></a>

        <div class="tip" style="margin-top: 14px;">Viewer URL</div>
        <a id="viewerUrlLink" class="url" href="#"></a>

        <div class="tip" style="margin-top: 14px;">Hotspot SSID</div>
        <div id="ssidText" class="mono"></div>

        <div class="tip" style="margin-top: 14px;">Hotspot Password</div>
        <div id="pwdText" class="mono"></div>

        <div class="actions">
          <button id="copyViewerBtn" type="button">Copy Viewer URL</button>
          <button id="openViewerBtn" type="button">Open Viewer URL</button>
          <button id="copySsidBtn" type="button">Copy SSID</button>
          <button id="copyPwdBtn" type="button">Copy Password</button>
          <button id="openWizardBtn" type="button">Open Share Wizard</button>
          <button id="openReadmeBtn" type="button">Open README</button>
          <button id="openDiagBtn" type="button">Open Diagnostics</button>
        </div>
      </section>

      <aside class="card poster">
        <div class="poster-hero">
          <div class="poster-title">Open on another device</div>
          <div class="tip">This page is fully local. The QR below is rendered from a local JavaScript bundle and its status is refreshed from <span class="mono">share_status.js</span>.</div>
          <div class="poster-url" id="posterUrl"></div>
          <div class="tip" style="margin-top:12px;">Viewer count: <span id="viewerBadgeText" class="accent">0</span></div>
        </div>

        <div id="qrMount" class="qr-shell" aria-live="polite">Loading local QR…</div>

        <div class="actions" style="margin-top:0;">
          <button id="downloadQrBtn" type="button">Download QR SVG</button>
          <button id="copyUrlBtn2" type="button">Copy Viewer URL</button>
        </div>

        <div>
          <div class="tip">How to connect</div>
          <ol>
            <li id="connectLine1"></li>
            <li>Scan the QR, or open a browser and enter the Viewer URL exactly as shown.</li>
            <li>If the browser warns about a self-signed certificate, trust the local host certificate for this session.</li>
            <li>If hosted-network control is unavailable on this device, use Windows Mobile Hotspot settings as the fallback path.</li>
          </ol>
        </div>
      </aside>
    </div>
  </main>

  <script id="bundleJson" type="application/json">)HTML";
    html << bundleJson;
    html << R"HTML(</script>
  <script src="./www/assets/share_card_qr.bundle.js"></script>
  <script>
    let state = JSON.parse(document.getElementById('bundleJson').textContent);
    let currentViewerUrl = '';
    let lastRenderedVersion = '';
    let pollTimer = null;

    function setText(id, text) {
      const el = document.getElementById(id);
      if (el) el.textContent = text == null ? '' : String(text);
    }

    function setLink(id, href) {
      const el = document.getElementById(id);
      if (!el) return;
      el.href = href || '#';
      el.textContent = href || '';
    }

    async function copyText(text, okLabel) {
      try {
        await navigator.clipboard.writeText(String(text || ''));
        alert(okLabel);
      } catch (_) {
        alert('Copy failed. Please copy it manually.');
      }
    }

    function currentStateVersion(bundle) {
      return [bundle.generatedAt || '', bundle.hostState || '', bundle.rooms || 0, bundle.viewers || 0, bundle.links && bundle.links.viewerUrl || ''].join('|');
    }

    function updateSyncLabel(source) {
      const now = new Date();
      const stamp = now.toLocaleTimeString();
      setText('lastSyncText', source + ' @ ' + stamp);
    }

    function renderQr(force) {
      const mount = document.getElementById('qrMount');
      if (!force && currentViewerUrl === (state.links && state.links.viewerUrl || '')) return;
      currentViewerUrl = state.links && state.links.viewerUrl || '';
      try {
        if (!window.LanShareQr) throw new Error('Local QR bundle is missing');
        mount.style.background = '#ffffff';
        mount.style.color = '#111827';
        window.LanShareQr.renderInto(mount, currentViewerUrl, { cellSize: 8, margin: 4 });
      } catch (err) {
        mount.style.background = '#161a22';
        mount.style.color = '#f2f3f5';
        mount.textContent = 'Local QR failed to render. Open the Viewer URL manually.\n' + String(err);
      }
    }

    function downloadQr() {
      try {
        const svg = window.LanShareQr.buildSvgMarkup(currentViewerUrl, { cellSize: 8, margin: 4 });
        const blob = new Blob([svg], { type: 'image/svg+xml;charset=utf-8' });
        const href = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = href;
        a.download = 'viewer_qr.svg';
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(href), 1000);
      } catch (err) {
        alert('QR export failed: ' + err);
      }
    }

    function renderBundle(bundle, source) {
      if (!bundle || !bundle.links) return;
      state = bundle;
      const hotspot = bundle.hotspot || {};
      const wifiDirect = bundle.wifiDirect || {};
      const viewerUrl = bundle.links.viewerUrl || '';
      const hostUrl = bundle.links.hostUrl || '';
      setText('modeText', bundle.networkMode || 'unknown');
      setText('hostIpText', bundle.hostIp || '(not found)');
      setText('portText', bundle.port || '');
      setText('roomText', bundle.room || '');
      setText('tokenText', bundle.token || '');
      setText('hostStateText', bundle.hostState || 'unknown');
      setText('roomsText', bundle.rooms || 0);
      setText('viewersText', bundle.viewers || 0);
      setText('hotspotStateText', hotspot.running ? 'running' : 'stopped');
      setText('wifiDirectApiText', wifiDirect.apiAvailable ? 'available' : 'not detected');
      setText('generatedAtText', bundle.generatedAt || '');
      setLink('hostUrlLink', hostUrl);
      setLink('viewerUrlLink', viewerUrl);
      setText('posterUrl', viewerUrl || '(viewer url missing)');
      setText('ssidText', hotspot.ssid || '(not configured)');
      setText('pwdText', hotspot.password || '(not configured)');
      setText('viewerBadgeText', bundle.viewers || 0);
      setText('connectLine1', hotspot.running
        ? 'Connect the phone or another PC to the hotspot shown on the left.'
        : 'Connect the phone or another PC to the same LAN / Wi-Fi as the host.');
      updateSyncLabel(source || 'embedded snapshot');
      const version = currentStateVersion(bundle);
      const forceQr = currentViewerUrl !== viewerUrl || lastRenderedVersion !== version;
      lastRenderedVersion = version;
      renderQr(forceQr);
    }

    function loadLiveStatus() {
      const script = document.createElement('script');
      script.src = './share_status.js?ts=' + Date.now();
      script.async = true;
      script.onload = () => {
        if (window.__LAN_SHARE_STATUS__) {
          renderBundle(window.__LAN_SHARE_STATUS__, 'share_status.js');
        }
        script.remove();
      };
      script.onerror = () => {
        setText('lastSyncText', 'share_status.js unavailable');
        script.remove();
      };
      document.head.appendChild(script);
    }

    window.addEventListener('lan-share-status', (ev) => {
      if (ev && ev.detail) renderBundle(ev.detail, 'share_status.js event');
    });

    document.getElementById('copyViewerBtn').onclick = () => copyText((state.links && state.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('copyUrlBtn2').onclick = () => copyText((state.links && state.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('openViewerBtn').onclick = () => window.open((state.links && state.links.viewerUrl) || '', '_blank');
    document.getElementById('copySsidBtn').onclick = () => copyText((state.hotspot && state.hotspot.ssid) || '', 'SSID copied');
    document.getElementById('copyPwdBtn').onclick = () => copyText((state.hotspot && state.hotspot.password) || '', 'Password copied');
    document.getElementById('openWizardBtn').onclick = () => window.open('./share_wizard.html', '_blank');
    document.getElementById('openReadmeBtn').onclick = () => window.open('./share_readme.txt', '_blank');
    document.getElementById('openDiagBtn').onclick = () => window.open('./share_diagnostics.txt', '_blank');
    document.getElementById('downloadQrBtn').onclick = downloadQr;

    renderBundle(state, 'embedded snapshot');
    pollTimer = window.setInterval(loadLiveStatus, 2000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) loadLiveStatus(); });
    window.addEventListener('focus', loadLiveStatus);
  </script>
</body>
</html>)HTML";

    return html.str();
}

static std::string JsonEscapeUtf8(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (unsigned char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return out;
}

static std::wstring BuildFileUrl(const fs::path& path) {
    std::wstring value = fs::absolute(path).wstring();
    std::replace(value.begin(), value.end(), L'\\', L'/');

    std::wstring encoded;
    encoded.reserve(value.size() + 16);
    for (wchar_t ch : value) {
        switch (ch) {
        case L' ':
            encoded += L"%20";
            break;
        case L'#':
            encoded += L"%23";
            break;
        case L'%':
            encoded += L"%25";
            break;
        default:
            encoded.push_back(ch);
            break;
        }
    }

    return L"file:///" + encoded;
}



struct SelfCheckItem {
    std::string id;
    std::string title;
    std::string status;
    std::string detail;
    std::string severity;
    std::string category;
    bool ok = false;
};

struct FailureHint {
    std::string title;
    std::string detail;
    std::string action;
    std::string severity;
    std::string category;
};

struct SelfCheckReport {
    std::vector<SelfCheckItem> items;
    std::vector<FailureHint> failures;
    int passed = 0;
    int total = 0;
    int p0 = 0;
    int p1 = 0;
    int p2 = 0;
    int certificateCount = 0;
    int networkCount = 0;
    int sharingCount = 0;
};

static std::string NormalizeSelfCheckSeverity(std::string_view severity) {
    const std::string sev(severity);
    if (sev == "P0" || sev == "p0") return "P0";
    if (sev == "P1" || sev == "p1") return "P1";
    return "P2";
}

static std::string NormalizeSelfCheckCategory(std::string_view category) {
    const std::string cat(category);
    if (cat == "certificate") return "certificate";
    if (cat == "network") return "network";
    return "sharing";
}

static std::string SeverityBadgeLabel(std::string_view severity) {
    const std::string sev = NormalizeSelfCheckSeverity(severity);
    if (sev == "P0") return "P0 blocker";
    if (sev == "P1") return "P1 fix soon";
    return "P2 advisory";
}

static void TouchSelfCheckCounters(SelfCheckReport& report,
                                   std::string_view severity,
                                   std::string_view category) {
    const std::string sev = NormalizeSelfCheckSeverity(severity);
    const std::string cat = NormalizeSelfCheckCategory(category);
    if (sev == "P0") ++report.p0;
    else if (sev == "P1") ++report.p1;
    else ++report.p2;

    if (cat == "certificate") ++report.certificateCount;
    else if (cat == "network") ++report.networkCount;
    else ++report.sharingCount;
}

static void AddSelfCheckItem(SelfCheckReport& report,
                             std::string id,
                             std::string title,
                             bool ok,
                             std::string okDetail,
                             std::string warnDetail,
                             std::string severity,
                             std::string category) {
    SelfCheckItem item;
    item.id = std::move(id);
    item.title = std::move(title);
    item.status = ok ? "ok" : "attention";
    item.detail = ok ? std::move(okDetail) : std::move(warnDetail);
    item.severity = NormalizeSelfCheckSeverity(severity);
    item.category = NormalizeSelfCheckCategory(category);
    item.ok = ok;
    report.items.push_back(std::move(item));
    ++report.total;
    if (ok) {
        ++report.passed;
    } else {
        TouchSelfCheckCounters(report, report.items.back().severity, report.items.back().category);
    }
}

static void AddFailureHint(SelfCheckReport& report,
                           std::string title,
                           std::string detail,
                           std::string action,
                           std::string severity,
                           std::string category) {
    FailureHint hint;
    hint.title = std::move(title);
    hint.detail = std::move(detail);
    hint.action = std::move(action);
    hint.severity = NormalizeSelfCheckSeverity(severity);
    hint.category = NormalizeSelfCheckCategory(category);
    report.failures.push_back(std::move(hint));
}

static SelfCheckReport BuildSelfCheckReport(std::wstring_view hostState,
                                            std::wstring_view hostIp,
                                            std::wstring_view viewerUrl,
                                            std::size_t viewers,
                                            bool serverProcessRunning,
                                            bool certFileExists,
                                            bool certKeyExists,
                                            bool portReady,
                                            std::wstring_view portDetail,
                                            bool localHealthReady,
                                            std::wstring_view localHealthDetail,
                                            bool hostIpReachable,
                                            std::wstring_view hostIpReachableDetail,
                                            bool lanBindReady,
                                            std::wstring_view lanBindDetail,
                                            int activeIpv4Candidates,
                                            bool selectedIpRecommended,
                                            std::wstring_view adapterHint,
                                            bool embeddedHostReady,
                                            std::wstring_view embeddedHostStatus,
                                            bool wifiAdapterPresent,
                                            bool hotspotSupported,
                                            bool wifiDirectApiAvailable,
                                            bool hotspotRunning,
                                            bool liveReady) {
    SelfCheckReport report;

    const bool serverReady = serverProcessRunning || IsHostStateServerRunning(hostState);
    const bool hostReady = IsHostStateReadyOrLoading(hostState);
    const bool hostSharing = IsHostStateSharing(hostState);
    const bool networkReady = !hostIp.empty() && hostIp != L"(not found)" && hostIp != L"0.0.0.0";
    const bool viewerReady = !viewerUrl.empty();
    const bool certReady = certFileExists && certKeyExists;
    const bool hotspotControlReady = hotspotSupported || hotspotRunning;
    const std::string portDetailUtf8 = urlutil::WideToUtf8(std::wstring(portDetail));
    const std::string localHealthDetailUtf8 = urlutil::WideToUtf8(std::wstring(localHealthDetail));
    const std::string hostIpReachableDetailUtf8 = urlutil::WideToUtf8(std::wstring(hostIpReachableDetail));
    const std::string lanBindDetailUtf8 = urlutil::WideToUtf8(std::wstring(lanBindDetail));
    const std::string adapterHintUtf8 = urlutil::WideToUtf8(std::wstring(adapterHint));
    const std::string embeddedHostStatusUtf8 = urlutil::WideToUtf8(std::wstring(embeddedHostStatus));

    AddSelfCheckItem(report,
                     "server-reachability",
                     "Server reachability",
                     serverReady,
                     "The desktop host reports the HTTPS/WSS server as running.",
                     "The desktop host is not reporting a running server. Press Start and wait for the host page to load.",
                     "P0",
                     "sharing");
    AddSelfCheckItem(report,
                     "listen-port",
                     "Listen port",
                     portReady,
                     portDetailUtf8.empty() ? "The configured TCP port looks available." : portDetailUtf8,
                     portDetailUtf8.empty()
                         ? "The configured TCP port is busy or invalid. Pick a free port before starting the host."
                         : portDetailUtf8,
                     "P0",
                     "network");
    AddSelfCheckItem(report,
                     "server-health-endpoint",
                     "Server health endpoint",
                     localHealthReady,
                     localHealthDetailUtf8.empty() ? "Local /health responded correctly." : localHealthDetailUtf8,
                     localHealthDetailUtf8.empty()
                         ? "The local HTTPS server did not respond to /health. Restart the server and re-run checks."
                         : localHealthDetailUtf8,
                     "P0",
                     "sharing");
    AddSelfCheckItem(report,
                     "lan-bind-address",
                     "LAN bind address",
                     lanBindReady,
                     lanBindDetailUtf8.empty() ? "Server bind address allows LAN clients." : lanBindDetailUtf8,
                     lanBindDetailUtf8.empty()
                         ? "Server bind address is loopback-only or invalid for LAN sharing."
                         : lanBindDetailUtf8,
                     "P0",
                     "network");
    AddSelfCheckItem(report,
                     "lan-entry-endpoint",
                     "LAN entry endpoint",
                     hostIpReachable,
                     hostIpReachableDetailUtf8.empty() ? "Selected host IP responded to /health." : hostIpReachableDetailUtf8,
                     hostIpReachableDetailUtf8.empty()
                         ? "The selected host IP did not answer the local /health probe."
                         : hostIpReachableDetailUtf8,
                     "P1",
                     "network");
    AddSelfCheckItem(report,
                     "adapter-selection",
                     "Adapter selection",
                     activeIpv4Candidates <= 1 || selectedIpRecommended,
                     adapterHintUtf8.empty() ? "The selected host IP matches the best active adapter." : adapterHintUtf8,
                     adapterHintUtf8.empty()
                         ? "Multiple adapters are active and the selected host IP may not be the best LAN target."
                         : adapterHintUtf8,
                     activeIpv4Candidates > 1 ? "P1" : "P2",
                     "network");
    AddSelfCheckItem(report,
                     "embedded-host-runtime",
                     "Embedded host runtime",
                     embeddedHostReady,
                     "Embedded host view is ready inside the desktop app.",
                     embeddedHostStatusUtf8 == "sdk-unavailable"
                         ? "WebView2 SDK/runtime support is unavailable in this build. Use Open Host Page in an external browser, or install a build with WebView2 support."
                         : embeddedHostStatusUtf8 == "runtime-unavailable" || embeddedHostStatusUtf8 == "controller-unavailable"
                               ? "WebView2 runtime did not initialize. The host page can still be opened in an external browser."
                               : embeddedHostStatusUtf8 == "initializing"
                                     ? "Embedded host view is still initializing. Wait a moment, then re-run checks."
                                     : "Embedded host view is unavailable. Use Open Host Page in an external browser as fallback.",
                     "P1",
                     "sharing");
    AddSelfCheckItem(report,
                     "host-sharing-state",
                     "Host sharing state",
                     hostSharing,
                     "Screen sharing is active in the embedded host page.",
                     !embeddedHostReady
                         ? "The embedded host page is unavailable inside the desktop app. Use Open Host Page in an external browser and start sharing there."
                         : hostReady
                         ? "The host page is loaded, but screen sharing does not look active yet. Click Start Share inside the host page."
                         : "The host page is not ready yet. Reopen the host page or restart the local server.",
                     "P1",
                     "sharing");
    AddSelfCheckItem(report,
                     "host-network",
                     "Host network",
                     networkReady,
                     std::string("Host IPv4 looks usable: ") + urlutil::WideToUtf8(std::wstring(hostIp)),
                     "Host IPv4 is missing. Refresh the IP, or start hotspot / join a LAN before exporting again.",
                     "P0",
                     "network");
    AddSelfCheckItem(report,
                     "viewer-entry-url",
                     "Viewer entry URL",
                     viewerReady,
                     "Viewer URL and QR target are present in the bundle.",
                     "Viewer URL is empty. Re-export after host IP and port are valid.",
                     "P1",
                     "sharing");
    AddSelfCheckItem(report,
                     "certificate-files",
                     "Certificate files",
                     certReady,
                     "Certificate files are present for the local HTTPS endpoint.",
                     "Certificate files are missing or incomplete. Start the local server once to generate them, then reopen the bundle.",
                     "P0",
                     "certificate");
    AddSelfCheckItem(report,
                     "wifi-adapter",
                     "Wi-Fi adapter",
                     wifiAdapterPresent,
                     "A Wi-Fi adapter is present on this machine.",
                     "No Wi-Fi adapter was detected. LAN sharing can still work, but hotspot and Wi-Fi Direct paths will be limited.",
                     "P2",
                     "network");
    AddSelfCheckItem(report,
                     "hotspot-control",
                     "Hotspot control",
                     hotspotControlReady,
                     hotspotRunning
                         ? "Hotspot support is active and the hotspot is currently running."
                         : "Hotspot control is available on this machine (best effort).",
                     wifiDirectApiAvailable
                         ? "Hosted-network control was not detected. Use Windows Mobile Hotspot settings or Wi-Fi Direct pairing as the fallback path."
                         : "Hosted-network control was not detected. Use Windows Mobile Hotspot settings as the fallback path.",
                     "P2",
                     "network");
    AddSelfCheckItem(report,
                     "live-bundle-refresh",
                     "Live bundle refresh",
                     liveReady,
                     "share_status.js is configured, so already-open pages can refresh in place.",
                     "Live status script is not configured; reopen the exported bundle after regenerating it.",
                     "P2",
                     "sharing");

    if (!serverReady) {
        AddFailureHint(report,
            "Server not running",
            "Press Start in the desktop host and wait for the host page to move out of the stopped state before asking a viewer to connect.",
            "Press Start in the desktop host, confirm Status becomes running, then let the embedded host page finish loading.",
            "P0",
            "sharing");
    }
    if (!portReady && !serverProcessRunning) {
        AddFailureHint(report,
            "Listen port unavailable",
            portDetailUtf8.empty()
                ? "The configured TCP port is busy or invalid, so the local HTTPS server cannot start on it."
                : portDetailUtf8,
            "Pick a different port or stop the process that already owns this port, then press Start again.",
            "P0",
            "network");
    }
    if (serverProcessRunning && !localHealthReady) {
        AddFailureHint(report,
            "Local health check failed",
            localHealthDetailUtf8.empty()
                ? "The server process started, but the local /health endpoint did not return a healthy response."
                : localHealthDetailUtf8,
            "Restart the local server. If the problem persists, inspect the server log and cert/www output beside the executable.",
            "P0",
            "sharing");
    }
    if (!lanBindReady) {
        AddFailureHint(report,
            "Server bind is not LAN-reachable",
            lanBindDetailUtf8.empty()
                ? "The desktop host is configured to bind only to loopback, so other devices cannot connect."
                : lanBindDetailUtf8,
            "Change the bind address to 0.0.0.0 or the selected LAN IP, then restart the server.",
            "P0",
            "network");
    }
    if (serverProcessRunning && networkReady && !hostIpReachable) {
        AddFailureHint(report,
            "Selected LAN address is not reachable",
            hostIpReachableDetailUtf8.empty()
                ? "The selected host IP did not answer the /health probe, so the exported Viewer URL is likely wrong for this machine."
                : hostIpReachableDetailUtf8,
            "Refresh Host IPv4, prefer the recommended active adapter, then restart the server or re-export the bundle.",
            "P1",
            "network");
    }
    if (!networkReady) {
        AddFailureHint(report,
            "Host IPv4 missing",
            "Refresh the detected IP or move the host onto a real LAN / hotspot before exporting the bundle again.",
            "Press Refresh beside Host IPv4. If it is still missing, connect the host to LAN or start the hotspot, then re-run checks.",
            "P0",
            "network");
    }
    if (!certReady) {
        AddFailureHint(report,
            "Certificate files missing",
            "The local HTTPS server expects self-signed certificate files in the cert folder. Start the host once so it can generate them, then trust the browser warning for this session.",
            "Start the local server once so cert/server.crt and cert/server.key are created, then reopen the bundle and trust the certificate warning for this session.",
            "P0",
            "certificate");
    }
    if (serverReady && !hostSharing && embeddedHostReady) {
        AddFailureHint(report,
            "Host page not sharing yet",
            "Open the embedded host page and complete the screen picker. Viewers can connect only after the host page starts media capture.",
            "In the embedded host page, click Start Share and finish the screen picker before sharing the Viewer URL.",
            "P1",
            "sharing");
    }
    if (!embeddedHostReady) {
        AddFailureHint(report,
            "Embedded host unavailable",
            embeddedHostStatusUtf8.empty()
                ? "The embedded host runtime is unavailable inside the desktop app."
                : "Embedded host runtime status: " + embeddedHostStatusUtf8 + ".",
            "Use Open Host Page to start sharing in an external browser, or install/repair the WebView2 runtime on this machine.",
            "P1",
            "sharing");
    }
    if (activeIpv4Candidates > 1 && !selectedIpRecommended) {
        AddFailureHint(report,
            "Multiple adapters are active",
            adapterHintUtf8.empty()
                ? "This machine has more than one active IPv4 adapter, so the exported share IP may not match the intended LAN."
                : adapterHintUtf8,
            "Use the recommended adapter IP for sharing, or disable the unrelated adapter before exporting diagnostics again.",
            "P1",
            "network");
    }
    if (viewerReady && !hotspotRunning && viewers == 0) {
        AddFailureHint(report,
            "Viewer may be on the wrong network",
            "Make sure the other device is on the same LAN / Wi-Fi. If not, start the hotspot or complete Wi-Fi Direct pairing first.",
            "Move the viewer onto the same LAN or join the hotspot shown in Share Info, then reopen the Viewer URL.",
            "P1",
            "network");
    }
    if (!wifiAdapterPresent) {
        AddFailureHint(report,
            "No Wi-Fi adapter detected",
            "Hotspot and Wi-Fi Direct flows may not work on this machine. Keep both devices on wired LAN / same router Wi-Fi instead.",
            "Prefer an existing wired LAN or the same router Wi-Fi for both devices; do not rely on hotspot on this machine.",
            "P2",
            "network");
    }
    if (!hotspotControlReady && !wifiDirectApiAvailable) {
        AddFailureHint(report,
            "No local wireless fallback detected",
            "Hosted-network control and Wi-Fi Direct API both look unavailable. Use an existing LAN/Wi-Fi or open Windows Mobile Hotspot settings for manual setup.",
            "Open Windows Mobile Hotspot settings for manual setup, or keep both devices on the same existing LAN/Wi-Fi.",
            "P2",
            "network");
    }

    return report;
}

static int SelfCheckSeveritySortKey(std::string_view severity) {
    const std::string sev = NormalizeSelfCheckSeverity(severity);
    if (sev == "P0") return 0;
    if (sev == "P1") return 1;
    return 2;
}

static std::vector<FailureHint> BuildOperatorFirstActions(const SelfCheckReport& report,
                                                          std::size_t maxCount = 3) {
    std::vector<FailureHint> actions = report.failures;
    std::stable_sort(actions.begin(), actions.end(), [](const FailureHint& a, const FailureHint& b) {
        const int ak = SelfCheckSeveritySortKey(a.severity);
        const int bk = SelfCheckSeveritySortKey(b.severity);
        if (ak != bk) return ak < bk;
        if (a.category != b.category) return a.category < b.category;
        return a.title < b.title;
    });
    if (actions.size() > maxCount) {
        actions.resize(maxCount);
    }
    return actions;
}

static std::wstring BuildMainDiagnosticSummaryText(const SelfCheckReport& report) {
    std::wstringstream ss;
    ss << L"Overall: " << report.passed << L" / " << report.total << L" checks passed\r\n";
    ss << L"Severity: P0 " << report.p0 << L" / P1 " << report.p1 << L" / P2 " << report.p2 << L"\r\n";
    ss << L"Categories: cert " << report.certificateCount << L" / net " << report.networkCount << L" / sharing " << report.sharingCount << L"\r\n";
    if (!report.failures.empty()) {
        const auto& top = report.failures.front();
        ss << L"Top issue: [" << urlutil::Utf8ToWide(top.severity) << L"][" << urlutil::Utf8ToWide(top.category)
           << L"] " << urlutil::Utf8ToWide(top.title) << L"\r\n";
    } else {
        ss << L"Top issue: none\r\n";
    }
    if (report.p0 > 0) {
        ss << L"Decision: fix P0 items before sending the bundle to viewers.";
    } else if (report.p1 > 0) {
        ss << L"Decision: bundle is usable, but clear the P1 items for a smoother handoff.";
    } else {
        ss << L"Decision: no blocker detected. You can share the current bundle.";
    }
    return ss.str();
}

static std::wstring BuildOperatorFirstActionsText(const SelfCheckReport& report) {
    const auto actions = BuildOperatorFirstActions(report, 3);
    std::wstringstream ss;
    if (actions.empty()) {
        ss << L"No urgent first action right now.\r\n";
        ss << L"1. Share the current bundle if the viewer is ready.\r\n";
        ss << L"2. Use Re-run Checks after any state change (server, IP, hotspot, sharing).";
        return ss.str();
    }

    for (std::size_t i = 0; i < actions.size(); ++i) {
        const auto& action = actions[i];
        ss << (i + 1) << L". [" << urlutil::Utf8ToWide(action.severity) << L"][" << urlutil::Utf8ToWide(action.category) << L"] "
           << urlutil::Utf8ToWide(action.title) << L"\r\n   "
           << urlutil::Utf8ToWide(action.action.empty() ? action.detail : action.action);
        if (i + 1 < actions.size()) ss << L"\r\n\r\n";
    }
    return ss.str();
}

static std::wstring BuildSelfCheckSummaryLine(const SelfCheckReport& report) {
    std::wstringstream ss;
    ss << report.passed << L" / " << report.total << L" ok";
    if (report.p0 > 0 || report.p1 > 0 || report.p2 > 0) {
        ss << L" | P0 " << report.p0 << L" / P1 " << report.p1 << L" / P2 " << report.p2;
    }
    if (!report.failures.empty()) {
        ss << L" | " << urlutil::Utf8ToWide(report.failures.front().title);
    }
    return ss.str();
}

static bool WideContainsCaseInsensitive(std::wstring_view text, std::wstring_view needle) {
    std::wstring hay(text);
    std::wstring ndl(needle);
    std::transform(hay.begin(), hay.end(), hay.begin(), [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
    std::transform(ndl.begin(), ndl.end(), ndl.begin(), [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
    return hay.find(ndl) != std::wstring::npos;
}

static void SetTextIfPresent(HWND hwnd, const std::wstring& value) {
    if (hwnd) SetWindowTextW(hwnd, value.c_str());
}

static std::wstring DetectLogLevel(std::wstring_view line) {
    if (WideContainsCaseInsensitive(line, L"failed") || WideContainsCaseInsensitive(line, L"error")) return L"Error";
    if (WideContainsCaseInsensitive(line, L"warn") || WideContainsCaseInsensitive(line, L"attention")) return L"Warning";
    return L"Info";
}

static std::wstring DetectLogSource(std::wstring_view line) {
    if (WideContainsCaseInsensitive(line, L"[host-page]") || WideContainsCaseInsensitive(line, L"webview")) return L"webview";
    if (WideContainsCaseInsensitive(line, L"network") || WideContainsCaseInsensitive(line, L"hotspot") || WideContainsCaseInsensitive(line, L"wifi")) return L"network";
    if (WideContainsCaseInsensitive(line, L"[spawn]") || WideContainsCaseInsensitive(line, L"/health") || WideContainsCaseInsensitive(line, L"server")) return L"server";
    return L"app";
}

static std::string BuildSelfCheckJson(const SelfCheckReport& report) {
    std::ostringstream json;
    json << "{\n";
    json << "    \"passed\": " << report.passed << ",\n";
    json << "    \"total\": " << report.total << ",\n";
    json << "    \"severityCounts\": {\n";
    json << "      \"P0\": " << report.p0 << ",\n";
    json << "      \"P1\": " << report.p1 << ",\n";
    json << "      \"P2\": " << report.p2 << "\n";
    json << "    },\n";
    json << "    \"categoryCounts\": {\n";
    json << "      \"certificate\": " << report.certificateCount << ",\n";
    json << "      \"network\": " << report.networkCount << ",\n";
    json << "      \"sharing\": " << report.sharingCount << "\n";
    json << "    },\n";
    json << "    \"items\": [\n";
    for (size_t i = 0; i < report.items.size(); ++i) {
        const auto& item = report.items[i];
        json << "      {\n";
        json << "        \"id\": \"" << JsonEscapeUtf8(item.id) << "\",\n";
        json << "        \"title\": \"" << JsonEscapeUtf8(item.title) << "\",\n";
        json << "        \"status\": \"" << JsonEscapeUtf8(item.status) << "\",\n";
        json << "        \"severity\": \"" << JsonEscapeUtf8(item.severity) << "\",\n";
        json << "        \"category\": \"" << JsonEscapeUtf8(item.category) << "\",\n";
        json << "        \"ok\": " << (item.ok ? "true" : "false") << ",\n";
        json << "        \"detail\": \"" << JsonEscapeUtf8(item.detail) << "\"\n";
        json << "      }" << (i + 1 < report.items.size() ? "," : "") << "\n";
    }
    json << "    ],\n";
    json << "    \"issues\": [\n";
    for (size_t i = 0; i < report.failures.size(); ++i) {
        const auto& item = report.failures[i];
        json << "      {\n";
        json << "        \"title\": \"" << JsonEscapeUtf8(item.title) << "\",\n";
        json << "        \"severity\": \"" << JsonEscapeUtf8(item.severity) << "\",\n";
        json << "        \"category\": \"" << JsonEscapeUtf8(item.category) << "\",\n";
        json << "        \"action\": \"" << JsonEscapeUtf8(item.action) << "\",\n";
        json << "        \"detail\": \"" << JsonEscapeUtf8(item.detail) << "\"\n";
        json << "      }" << (i + 1 < report.failures.size() ? "," : "") << "\n";
    }
    json << "    ]\n";
    json << "  }";
    return json.str();
}

static std::string BuildDesktopSelfCheckHtml(std::string_view bundleJson) {
    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>LAN Screen Share - Desktop Self-Check</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin:0; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; background:#0b1020; color:#edf2f7; }
    .page { max-width: 1180px; margin:0 auto; padding:24px; }
    .hero { display:grid; grid-template-columns:1.15fr .85fr; gap:18px; margin-bottom:20px; }
    .card { background:#111827; border:1px solid #23304a; border-radius:18px; padding:20px; box-shadow:0 10px 30px rgba(0,0,0,.24); }
    .pill { display:inline-flex; align-items:center; gap:8px; border-radius:999px; padding:6px 10px; background:#18243b; border:1px solid #30415f; color:#dbeafe; font-size:12px; }
    .pill.ok { background:#0f2b1f; border-color:#1f6f43; color:#d1fae5; }
    .pill.warn { background:#2d1e08; border-color:#7c5d1a; color:#fde68a; }
    .pill.p0 { background:#35121a; border-color:#9f1239; color:#fecdd3; }
    .pill.p1 { background:#312113; border-color:#b45309; color:#fde68a; }
    .pill.p2 { background:#15253c; border-color:#2563eb; color:#bfdbfe; }
    .pill.certificate { background:#1f1835; border-color:#7c3aed; color:#ddd6fe; }
    .pill.network { background:#0f2530; border-color:#0891b2; color:#bae6fd; }
    .pill.sharing { background:#172436; border-color:#4f46e5; color:#c7d2fe; }
    .sub { color:#9fb0c8; line-height:1.6; }
    .kv { display:grid; grid-template-columns:140px 1fr; gap:8px 12px; margin-top:14px; }
    .kv div:nth-child(odd) { color:#9fb0c8; }
    .mono { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; overflow-wrap:anywhere; }
    .checks { display:grid; grid-template-columns:repeat(2, minmax(0,1fr)); gap:12px; }
    .check { border:1px solid #263656; border-radius:14px; padding:14px; background:#0d172b; }
    .check.ok { border-color:#1f6f43; background:#0d2218; }
    .check.warn { border-color:#7c5d1a; background:#231a09; }
    .issues { margin:0; padding-left:18px; }
    .issues li { margin:10px 0; }
    .actions, .filter-row, .summary-row { display:flex; flex-wrap:wrap; gap:10px; margin-top:16px; align-items:center; }
    button, a.btn { appearance:none; text-decoration:none; border:1px solid #334766; background:#16233a; color:#fff; padding:10px 14px; border-radius:12px; cursor:pointer; display:inline-flex; align-items:center; gap:8px; }
    button.filter { padding:8px 12px; font-size:12px; border-radius:999px; background:#10192b; }
    button.filter.active { background:#244064; border-color:#7dd3fc; color:#e0f2fe; }
    .muted { opacity:.78; }
    .hidden { display:none !important; }
    .check-head { display:flex; justify-content:space-between; gap:12px; align-items:flex-start; margin-bottom:10px; }
    .check-meta { display:flex; flex-wrap:wrap; gap:8px; justify-content:flex-end; }
    .section-title { margin:0 0 10px; }
    .small-gap { margin-top:8px; }
    @media (max-width: 920px) { .hero, .checks { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <main class="page">
    <section class="hero">
      <div class="card">
        <div class="summary-row">
          <span class="pill">Desktop self-check</span>
          <span class="pill" id="summaryPill">Running…</span>
          <span class="sub">Last sync: <span id="lastSyncText">embedded snapshot</span></span>
        </div>
        <h1 style="margin:12px 0;">Desktop self-check report</h1>
        <div class="sub">This page uses the same exported bundle data as Share Wizard, but focuses on host-side troubleshooting from the desktop operator view. The checks below now carry a P0 / P1 / P2 severity and a certificate / network / sharing category.</div>
        <div class="kv">
          <div>Host state</div><div id="hostStateText"></div>
          <div>Host IPv4</div><div id="hostIpText"></div>
          <div>Room / viewers</div><div><span id="roomText"></span> / <span id="viewersText"></span></div>
          <div>Viewer URL</div><div id="viewerUrlText" class="mono"></div>
          <div>Hotspot</div><div id="hotspotText"></div>
          <div>Certificate</div><div id="certText"></div>
        </div>
        <div class="summary-row">
          <span class="pill p0" id="p0Pill">P0: 0</span>
          <span class="pill p1" id="p1Pill">P1: 0</span>
          <span class="pill p2" id="p2Pill">P2: 0</span>
          <span class="pill certificate" id="certPill">Certificate: 0</span>
          <span class="pill network" id="networkPill">Network: 0</span>
          <span class="pill sharing" id="sharingPill">Sharing: 0</span>
        </div>
        <div class="actions">
          <a class="btn" href="./share_wizard.html" target="_blank" rel="noopener">Open Share Wizard</a>
          <a class="btn" href="./share_card.html" target="_blank" rel="noopener">Open Share Card</a>
          <a class="btn" href="./share_diagnostics.txt" target="_blank" rel="noopener">Open Diagnostics TXT</a>
          <button id="copyViewerBtn" type="button">Copy Viewer URL</button>
        </div>
      </div>
      <div class="card">
        <h2 class="section-title">Self-check summary</h2>
        <div class="sub">Passed: <span id="summaryText">0 / 0</span></div>
        <div class="small-gap sub" id="filterSummaryText">Showing all checks</div>
        <ul id="issueList" class="issues"></ul>
        <h2 class="section-title" style="margin-top:18px;">Suggested next actions</h2>
        <div class="sub">These now consume the exported <span class="mono">selfCheck.issues[].action</span> field first, so the desktop report, Share Wizard, and text diagnostics stay aligned.</div>
        <ul id="actionList" class="issues"></ul>
      </div>
    </section>

    <section class="card" style="margin-bottom:20px;">
      <h2 class="section-title">Filter view</h2>
      <div class="sub">Use these to focus on one problem class or one severity tier when you are troubleshooting with someone remotely.</div>
      <div class="filter-row">
        <strong>Category</strong>
        <button class="filter active" type="button" data-filter-kind="category" data-filter-value="all">All</button>
        <button class="filter" type="button" data-filter-kind="category" data-filter-value="certificate">Certificate</button>
        <button class="filter" type="button" data-filter-kind="category" data-filter-value="network">Network</button>
        <button class="filter" type="button" data-filter-kind="category" data-filter-value="sharing">Sharing</button>
      </div>
      <div class="filter-row">
        <strong>Severity</strong>
        <button class="filter active" type="button" data-filter-kind="severity" data-filter-value="all">All</button>
        <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P0">P0</button>
        <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P1">P1</button>
        <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P2">P2</button>
      </div>
    </section>

    <section class="card">
      <h2 class="section-title">Checks</h2>
      <div id="checkGrid" class="checks"></div>
    </section>
  </main>

  <script id="bundleJson" type="application/json">)HTML";
    html << bundleJson;
    html << R"HTML(</script>
  <script>
    let bundle = JSON.parse(document.getElementById('bundleJson').textContent);
    const activeFilters = { category: 'all', severity: 'all' };

    function setText(id, text) {
      const el = document.getElementById(id);
      if (el) el.textContent = text == null ? '' : String(text);
    }

    function htmlEscape(v) {
      return String(v == null ? '' : v)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;')
        .replaceAll("'", '&#39;');
    }

    function normalizeSeverity(v) {
      return v === 'P0' || v === 'P1' ? v : 'P2';
    }

    function normalizeCategory(v) {
      return v === 'certificate' || v === 'network' ? v : 'sharing';
    }

    function severityClass(v) {
      return normalizeSeverity(v).toLowerCase();
    }

    function categoryClass(v) {
      return normalizeCategory(v);
    }

    async function copyViewerUrl() {
      try {
        await navigator.clipboard.writeText((bundle.links && bundle.links.viewerUrl) || '');
        alert('Viewer URL copied');
      } catch (_) {
        alert('Copy failed. Please copy it manually.');
      }
    }

    function updateSyncLabel(source) {
      const now = new Date();
      setText('lastSyncText', source + ' @ ' + now.toLocaleTimeString());
    }

    function updateSummaryPills(report) {
      const sev = report.severityCounts || {};
      const cat = report.categoryCounts || {};
      setText('p0Pill', 'P0: ' + (sev.P0 || 0));
      setText('p1Pill', 'P1: ' + (sev.P1 || 0));
      setText('p2Pill', 'P2: ' + (sev.P2 || 0));
      setText('certPill', 'Certificate: ' + (cat.certificate || 0));
      setText('networkPill', 'Network: ' + (cat.network || 0));
      setText('sharingPill', 'Sharing: ' + (cat.sharing || 0));
    }

    function filterItems(items) {
      return (items || []).filter(item => {
        const categoryOk = activeFilters.category === 'all' || normalizeCategory(item.category) === activeFilters.category;
        const severityOk = activeFilters.severity === 'all' || normalizeSeverity(item.severity) === activeFilters.severity;
        return categoryOk && severityOk;
      });
    }
    function sortIssuesByPriority(items) {
      return (items || []).slice().sort((a, b) => {
        const sevOrder = { P0: 0, P1: 1, P2: 2 };
        const ak = sevOrder[normalizeSeverity(a.severity)] ?? 9;
        const bk = sevOrder[normalizeSeverity(b.severity)] ?? 9;
        if (ak !== bk) return ak - bk;
        const ac = normalizeCategory(a.category);
        const bc = normalizeCategory(b.category);
        if (ac !== bc) return ac.localeCompare(bc);
        return String(a.title || '').localeCompare(String(b.title || ''));
      });
    }

    function normalizeIssues(issues) {
      return sortIssuesByPriority((issues || []).map(item => ({
        title: item.title,
        detail: item.detail,
        action: item.action,
        category: normalizeCategory(item.category),
        severity: normalizeSeverity(item.severity)
      })));
    }

    function renderIssues(issues) {
      const root = document.getElementById('issueList');
      if (!root) return;
      const filtered = filterItems(normalizeIssues(issues));
      if (!filtered.length) {
        root.innerHTML = '<li><strong>No matching issue in this filter view.</strong> Switch back to All to review the full troubleshooting list.</li>';
        return;
      }
      root.innerHTML = filtered.map(item => {
        const actionLine = item.action ? '<div class="small-gap muted"><strong>Action:</strong> ' + htmlEscape(item.action) + '</div>' : '';
        return '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(item.detail) +
          actionLine +
          ' <span class="pill ' + severityClass(item.severity) + '">' + htmlEscape(item.severity) + '</span>' +
          ' <span class="pill ' + categoryClass(item.category) + '">' + htmlEscape(item.category) + '</span></li>';
      }).join('');
    }

    function renderActionItems(issues) {
      const root = document.getElementById('actionList');
      if (!root) return;
      const filtered = filterItems(normalizeIssues(issues)).filter(item => !!(item.action || item.detail));
      if (!filtered.length) {
        root.innerHTML = '<li><strong>No urgent action needed.</strong> The exported issue list does not currently contain a blocking action. You can share the current bundle or use Re-run Checks after any state change.</li>';
        return;
      }
      root.innerHTML = filtered.map(item =>
        '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(item.action || item.detail) +
        ' <span class="pill ' + severityClass(item.severity) + '">' + htmlEscape(item.severity) + '</span>' +
        ' <span class="pill ' + categoryClass(item.category) + '">' + htmlEscape(item.category) + '</span></li>').join('');
    }

    function renderChecks(items) {
      const root = document.getElementById('checkGrid');
      if (!root) return;
      const filtered = filterItems(items || []);
      if (!filtered.length) {
        root.innerHTML = '<div class="check warn"><div class="check-head"><strong>No checks match the current filters.</strong></div><div class="sub">Clear one filter or switch back to All to see the full report.</div></div>';
        return;
      }
      root.innerHTML = filtered.map(item => {
        const status = item.ok ? 'ok' : 'warn';
        const severity = normalizeSeverity(item.severity);
        const category = normalizeCategory(item.category);
        return '<article class="check ' + status + '">' +
          '<div class="check-head">' +
          '<strong>' + htmlEscape(item.title) + '</strong>' +
          '<div class="check-meta">' +
          '<span class="pill ' + status + '">' + htmlEscape(item.status || (item.ok ? 'ok' : 'attention')) + '</span>' +
          '<span class="pill ' + severityClass(severity) + '">' + htmlEscape(severity) + '</span>' +
          '<span class="pill ' + categoryClass(category) + '">' + htmlEscape(category) + '</span>' +
          '</div></div>' +
          '<div class="sub">' + htmlEscape(item.detail) + '</div>' +
          '</article>';
      }).join('');
    }

    function updateFilterSummary(items) {
      const filtered = filterItems(items || []);
      const categoryText = activeFilters.category === 'all' ? 'all categories' : activeFilters.category;
      const severityText = activeFilters.severity === 'all' ? 'all severities' : activeFilters.severity;
      setText('filterSummaryText', 'Showing ' + filtered.length + ' checks for ' + categoryText + ' / ' + severityText + '.');
    }

    function applyFilters(data) {
      const selfCheck = data.selfCheck || {};
      updateFilterSummary(selfCheck.items || []);
      renderChecks(selfCheck.items || []);
      renderIssues(selfCheck.issues || []);
      renderActionItems(selfCheck.issues || []);
    }

    function bindFilters() {
      document.querySelectorAll('button.filter').forEach(btn => {
        btn.onclick = () => {
          const kind = btn.getAttribute('data-filter-kind');
          const value = btn.getAttribute('data-filter-value');
          activeFilters[kind] = value;
          document.querySelectorAll('button.filter[data-filter-kind="' + kind + '"]').forEach(x => x.classList.toggle('active', x === btn));
          applyFilters(bundle);
        };
      });
    }

    function renderBundle(data, source) {
      if (!data) return;
      bundle = data;
      const selfCheck = data.selfCheck || {};
      const hotspot = data.hotspot || {};
      const cert = data.cert || {};
      setText('hostStateText', data.hostState || 'unknown');
      setText('hostIpText', data.hostIp || '(not found)');
      setText('roomText', data.room || '');
      setText('viewersText', data.viewers || 0);
      setText('viewerUrlText', (data.links && data.links.viewerUrl) || '');
      setText('hotspotText', (hotspot.running ? 'running' : 'stopped') + (hotspot.ssid ? ' · ' + hotspot.ssid : ''));
      setText('certText', cert.ready ? 'ready' : 'missing / incomplete');
      setText('summaryText', (selfCheck.passed || 0) + ' / ' + (selfCheck.total || 0));
      const pill = document.getElementById('summaryPill');
      if (pill) {
        const sev = selfCheck.severityCounts || {};
        const hasWarn = (sev.P0 || 0) + (sev.P1 || 0) + (sev.P2 || 0) > 0;
        pill.className = 'pill ' + (hasWarn ? 'warn' : 'ok');
        pill.textContent = hasWarn ? 'Needs attention' : 'Healthy snapshot';
      }
      updateSummaryPills(selfCheck);
      applyFilters(data);
      updateSyncLabel(source || 'embedded snapshot');
    }

    function loadLiveStatus() {
      const script = document.createElement('script');
      script.src = './share_status.js?ts=' + Date.now();
      script.onload = () => {
        script.remove();
        if (window.__LAN_SHARE_STATUS__) renderBundle(window.__LAN_SHARE_STATUS__, 'share_status.js');
      };
      script.onerror = () => {
        script.remove();
        setText('lastSyncText', 'share_status.js unavailable');
      };
      document.head.appendChild(script);
    }

    window.addEventListener('lan-share-status', (ev) => {
      if (ev && ev.detail) renderBundle(ev.detail, 'share_status.js event');
    });

    document.getElementById('copyViewerBtn').onclick = copyViewerUrl;
    bindFilters();
    renderBundle(bundle, 'embedded snapshot');
    window.setInterval(loadLiveStatus, bundle.refreshMs || 2000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) loadLiveStatus(); });
    window.addEventListener('focus', loadLiveStatus);
  </script>
</body>
</html>)HTML";
    return html.str();
}

static std::string BuildDesktopSelfCheckText(std::wstring_view generatedAt,
                                             std::wstring_view hostUrl,
                                             std::wstring_view viewerUrl,
                                             const SelfCheckReport& report) {
    std::ostringstream out;
    out << "LAN Screen Share - Desktop self-check\r\n";
    out << "=====================================\r\n\r\n";
    out << "Generated: " << urlutil::WideToUtf8(std::wstring(generatedAt)) << "\r\n";
    out << "Host URL: " << urlutil::WideToUtf8(std::wstring(hostUrl)) << "\r\n";
    out << "Viewer URL: " << urlutil::WideToUtf8(std::wstring(viewerUrl)) << "\r\n\r\n";
    out << "Summary: " << report.passed << " / " << report.total << " checks passed\r\n";
    out << "Severity: P0=" << report.p0 << ", P1=" << report.p1 << ", P2=" << report.p2 << "\r\n";
    out << "Categories: certificate=" << report.certificateCount << ", network=" << report.networkCount << ", sharing=" << report.sharingCount << "\r\n\r\n";
    out << "Checks\r\n";
    out << "------\r\n";
    for (const auto& item : report.items) {
        out << "- [" << item.severity << "][" << item.category << "] " << item.title << ": " << item.status << "\r\n";
        out << "  " << item.detail << "\r\n";
    }
    out << "\r\nOpen issues\r\n";
    out << "-----------\r\n";
    if (report.failures.empty()) {
        out << "- No obvious blocking issue detected.\r\n";
    } else {
        for (const auto& issue : report.failures) {
            out << "- [" << issue.severity << "][" << issue.category << "] " << issue.title << ": " << issue.detail << "\r\n";
        }
    }
    out << "\r\nOperator first actions\r\n";
    out << "----------------------\r\n";
    const auto actions = BuildOperatorFirstActions(report, 3);
    if (actions.empty()) {
        out << "- No urgent first action right now. Share the current bundle or re-run checks after any state change.\r\n";
    } else {
        for (const auto& action : actions) {
            out << "- [" << action.severity << "][" << action.category << "] " << action.title << ": "
                << (action.action.empty() ? action.detail : action.action) << "\r\n";
        }
    }
    out << "\r\nNotes\r\n";
    out << "-----\r\n";
    out << "- This report is generated from the same bundle snapshot used by Share Wizard.\r\n";
    out << "- P0 means blocker, P1 means fix soon, P2 means advisory/fallback.\r\n";
    out << "- Re-run the self-check after host IP, server, hotspot, or sharing state changes.\r\n";
    return out.str();
}

static std::string BuildShareBundleJson(std::wstring_view networkMode,
                                        std::wstring_view hostIp,
                                        int port,
                                        std::wstring_view room,
                                        std::wstring_view token,
                                        std::wstring_view hostState,
                                        std::size_t rooms,
                                        std::size_t viewers,
                                        std::wstring_view hotspotSsid,
                                        std::wstring_view hotspotPassword,
                                        bool hotspotRunning,
                                        bool wifiAdapterPresent,
                                        bool hotspotSupported,
                                        bool wifiDirectApiAvailable,
                                        std::wstring_view wifiDirectAlias,
                                        std::wstring_view hostUrl,
                                        std::wstring_view viewerUrl,
                                        std::wstring_view generatedAt,
                                        bool serverProcessRunning,
                                        bool portReady,
                                        std::wstring_view portDetail,
                                        bool localHealthReady,
                                        std::wstring_view localHealthDetail,
                                        bool hostIpReachable,
                                        std::wstring_view hostIpReachableDetail,
                                        bool lanBindReady,
                                        std::wstring_view lanBindDetail,
                                        int activeIpv4Candidates,
                                        bool selectedIpRecommended,
                                        std::wstring_view adapterHint,
                                        bool embeddedHostReady,
                                        std::wstring_view embeddedHostStatus,
                                        std::wstring_view certDir,
                                        std::wstring_view certFile,
                                        std::wstring_view certKeyFile,
                                        bool certFileExists,
                                        bool certKeyExists) {
    auto q = [](std::wstring_view value) {
        return std::string(1, '"') + JsonEscapeUtf8(urlutil::WideToUtf8(std::wstring(value))) + std::string(1, '"');
    };

    const bool serverRunning = serverProcessRunning || IsHostStateServerRunning(hostState);
    const bool hostReady = IsHostStateReadyOrLoading(hostState);
    const bool hostSharing = IsHostStateSharing(hostState);
    const bool networkReady = !hostIp.empty() && hostIp != L"(not found)" && hostIp != L"0.0.0.0";
    const bool viewerReady = !viewerUrl.empty();
    const bool certReady = certFileExists && certKeyExists;
    const auto report = BuildSelfCheckReport(hostState, hostIp, viewerUrl, viewers, serverProcessRunning,
                                             certFileExists, certKeyExists, portReady, portDetail,
                                             localHealthReady, localHealthDetail, hostIpReachable, hostIpReachableDetail,
                                             lanBindReady, lanBindDetail, activeIpv4Candidates, selectedIpRecommended, adapterHint,
                                             embeddedHostReady, embeddedHostStatus,
                                             wifiAdapterPresent, hotspotSupported, wifiDirectApiAvailable, hotspotRunning, true);

    std::ostringstream json;
    json << "{\n";
    json << R"JSON(  "generatedAt": )JSON" << q(generatedAt) << ",\n";
    json << R"JSON(  "statusVersion": )JSON" << q(generatedAt) << ",\n";
    json << R"JSON(  "refreshMs": 2000,
)JSON";
    json << R"JSON(  "serverRunning": )JSON" << (serverRunning ? "true" : "false") << ",\n";
    json << R"JSON(  "networkMode": )JSON" << q(networkMode) << ",\n";
    json << R"JSON(  "hostIp": )JSON" << q(hostIp) << ",\n";
    json << R"JSON(  "port": )JSON" << port << ",\n";
    json << R"JSON(  "room": )JSON" << q(room) << ",\n";
    json << R"JSON(  "token": )JSON" << q(token) << ",\n";
    json << R"JSON(  "hostState": )JSON" << q(hostState) << ",\n";
    json << R"JSON(  "rooms": )JSON" << rooms << ",\n";
    json << R"JSON(  "viewers": )JSON" << viewers << ",\n";
    json << R"JSON(  "hotspot": {
    "running": )JSON" << (hotspotRunning ? "true" : "false") << ",\n";
    json << R"JSON(    "status": )JSON" << (hotspotRunning ? "\"running\"" : "\"stopped\"") << ",\n";
    json << R"JSON(    "supported": )JSON" << (hotspotSupported ? "true" : "false") << ",\n";
    json << R"JSON(    "ssid": )JSON" << q(hotspotSsid) << ",\n";
    json << R"JSON(    "password": )JSON" << q(hotspotPassword) << "\n";
    json << R"JSON(  },
  "wifi": {
    "adapterPresent": )JSON" << (wifiAdapterPresent ? "true" : "false") << "\n";
    json << R"JSON(  },
  "wifiDirect": {
    "apiAvailable": )JSON" << (wifiDirectApiAvailable ? "true" : "false") << ",\n";
    json << R"JSON(    "sessionAlias": )JSON" << q(wifiDirectAlias) << ",\n";
    json << R"JSON(    "pairingEntry": "ms-settings:connecteddevices",
    "fallbackEntry": "control.exe /name Microsoft.DevicesAndPrinters"
  },
  "cert": {
    "dir": )JSON" << q(certDir) << ",\n";
    json << R"JSON(    "certFile": )JSON" << q(certFile) << ",\n";
    json << R"JSON(    "keyFile": )JSON" << q(certKeyFile) << ",\n";
    json << R"JSON(    "certExists": )JSON" << (certFileExists ? "true" : "false") << ",\n";
    json << R"JSON(    "keyExists": )JSON" << (certKeyExists ? "true" : "false") << ",\n";
    json << R"JSON(    "ready": )JSON" << (certReady ? "true" : "false") << ",\n";
    json << R"JSON(    "trustHint": "If the browser warns about a self-signed certificate, trust it for this local session."
  },
  "checks": {
    "serverReady": )JSON" << (serverRunning ? "true" : "false") << ",\n";
    json << R"JSON(    "hostReady": )JSON" << (hostReady ? "true" : "false") << ",\n";
    json << R"JSON(    "hostSharing": )JSON" << (hostSharing ? "true" : "false") << ",\n";
    json << R"JSON(    "portReady": )JSON" << (portReady ? "true" : "false") << ",\n";
    json << R"JSON(    "localHealthReady": )JSON" << (localHealthReady ? "true" : "false") << ",\n";
    json << R"JSON(    "hostIpReachable": )JSON" << (hostIpReachable ? "true" : "false") << ",\n";
    json << R"JSON(    "lanBindReady": )JSON" << (lanBindReady ? "true" : "false") << ",\n";
    json << R"JSON(    "selectedIpRecommended": )JSON" << (selectedIpRecommended ? "true" : "false") << ",\n";
    json << R"JSON(    "embeddedHostReady": )JSON" << (embeddedHostReady ? "true" : "false") << ",\n";
    json << R"JSON(    "networkReady": )JSON" << (networkReady ? "true" : "false") << ",\n";
    json << R"JSON(    "viewerReady": )JSON" << (viewerReady ? "true" : "false") << ",\n";
    json << R"JSON(    "certificateReady": )JSON" << (certReady ? "true" : "false") << ",\n";
    json << R"JSON(    "bundleLiveReady": true
  },
  "runtime": {
    "portDetail": )JSON" << q(portDetail) << ",\n";
    json << R"JSON(    "localHealthDetail": )JSON" << q(localHealthDetail) << ",\n";
    json << R"JSON(    "hostIpReachableDetail": )JSON" << q(hostIpReachableDetail) << ",\n";
    json << R"JSON(    "lanBindDetail": )JSON" << q(lanBindDetail) << ",\n";
    json << R"JSON(    "activeIpv4Candidates": )JSON" << activeIpv4Candidates << ",\n";
    json << R"JSON(    "adapterHint": )JSON" << q(adapterHint) << ",\n";
    json << R"JSON(    "embeddedHostStatus": )JSON" << q(embeddedHostStatus) << "\n";
    json << R"JSON(  },
  "selfCheck": )JSON" << BuildSelfCheckJson(report) << ",\n";
    json << R"JSON(  "bundle": {
    "statusScript": "./share_status.js",
    "readme": "./share_readme.txt",
    "diagnostics": "./share_diagnostics.txt",
    "desktopSelfCheck": "./desktop_self_check.html",
    "desktopSelfCheckText": "./desktop_self_check.txt"
  },
  "links": {
    "hostUrl": )JSON" << q(hostUrl) << ",\n";
    json << R"JSON(    "viewerUrl": )JSON" << q(viewerUrl) << "\n";
    json << R"JSON(  }
}
)JSON";
    return json.str();
}

static std::string BuildShareWizardHtml(std::string_view bundleJson) {
    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>LAN Screen Share - Share Wizard</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; background: #0b1020; color: #edf2f7; }
    .page { max-width: 1180px; margin: 0 auto; padding: 24px; }
    .hero { display:grid; grid-template-columns: 1.4fr .9fr; gap:18px; margin-bottom:20px; }
    .card { background:#111827; border:1px solid #23304a; border-radius:20px; padding:20px; box-shadow:0 10px 30px rgba(0,0,0,.24); }
    h1,h2,h3 { margin:0 0 12px; }
    .sub { color:#9fb0c8; line-height:1.6; }
    .pill { display:inline-flex; align-items:center; gap:8px; border-radius:999px; padding:6px 10px; background:#18243b; border:1px solid #30415f; color:#dbeafe; font-size:12px; }
    .pill.ok { background:#0f2b1f; border-color:#1f6f43; color:#d1fae5; }
    .pill.warn { background:#2d1e08; border-color:#7c5d1a; color:#fde68a; }
    .pill.p0 { background:#35121a; border-color:#9f1239; color:#fecdd3; }
    .pill.p1 { background:#312113; border-color:#b45309; color:#fde68a; }
    .pill.p2 { background:#15253c; border-color:#2563eb; color:#bfdbfe; }
    .pill.certificate { background:#1f1835; border-color:#7c3aed; color:#ddd6fe; }
    .pill.network { background:#0f2530; border-color:#0891b2; color:#bae6fd; }
    .pill.sharing { background:#172436; border-color:#4f46e5; color:#c7d2fe; }
    .grid2 { display:grid; grid-template-columns:1fr 1fr; gap:18px; }
    .grid3 { display:grid; grid-template-columns:repeat(3,1fr); gap:14px; }
    .kv { display:grid; grid-template-columns:140px 1fr; gap:8px 12px; margin-top:12px; }
    .kv div:nth-child(odd) { color:#9fb0c8; }
    .mono { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; overflow-wrap:anywhere; }
    .box { background:#0b1222; border:1px solid #23304a; border-radius:14px; padding:14px; }
    .step { border:1px solid #263656; border-radius:16px; padding:16px; background:#0d172b; }
    .step.ok { border-color:#1f6f43; background:#0d2218; }
    .step.warn { border-color:#7c5d1a; background:#231a09; }
    .title-row { display:flex; gap:10px; align-items:center; justify-content:space-between; }
    .actions { display:flex; flex-wrap:wrap; gap:10px; margin-top:16px; }
    button, a.btn { appearance:none; text-decoration:none; border:1px solid #334766; background:#16233a; color:#fff; padding:10px 14px; border-radius:12px; cursor:pointer; display:inline-flex; align-items:center; gap:8px; }
    button.filter { padding:8px 12px; border-radius:999px; font-size:12px; background:#10192b; }
    button.filter.active { background:#244064; border-color:#7dd3fc; color:#e0f2fe; }
    .small { font-size:13px; color:#9fb0c8; line-height:1.6; }
    .qr { min-height:280px; display:flex; align-items:center; justify-content:center; background:#fff; border-radius:16px; padding:16px; }
    .qr svg { width:min(100%, 280px); height:auto; display:block; }
    .livebar { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-bottom:12px; }
    .em { color:#7dd3fc; }
    .diag-grid { display:grid; grid-template-columns:repeat(2, minmax(0,1fr)); gap:12px; margin-top:14px; }
    .diag-item { border:1px solid #263656; border-radius:14px; padding:14px; background:#0d172b; }
    .diag-item.ok { border-color:#1f6f43; background:#0d2218; }
    .diag-item.warn { border-color:#7c5d1a; background:#231a09; }
    .diag-item h3 { font-size:15px; margin-bottom:8px; }
    .checkline { display:flex; align-items:center; justify-content:space-between; gap:12px; }
    .issues, .action-list { margin:0; padding-left:18px; }
    .issues li, .action-list li { margin:10px 0; color:#dbe4f3; }
    .filter-row, .summary-row { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-top:12px; }
    .issues strong, .action-list strong { color:#fff; }
    ol { margin:10px 0 0 18px; padding:0; }
    li { margin:8px 0; }
    @media (max-width: 980px) { .hero, .grid2, .grid3, .diag-grid { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <main class="page">
    <section class="hero">
      <div class="card">
        <div class="livebar">
          <div class="pill">Local share wizard + local QR + offline bundle</div>
          <div class="pill">Live auto-refresh: <span id="liveStatusText">enabled</span></div>
          <div class="small">Last sync: <span id="lastSyncText">embedded snapshot</span></div>
        </div>
        <h1>LAN Screen Share Wizard</h1>
        <p class="sub">This page is generated locally by the desktop host. It packages the current room, token, URLs, certificate hints, hotspot details, and Wi-Fi Direct guidance into one offline handoff bundle.</p>
        <div class="kv">
          <div>Mode</div><div id="modeText"></div>
          <div>Host IPv4</div><div id="hostIpText" class="mono"></div>
          <div>Port</div><div id="portText"></div>
          <div>Room</div><div id="roomText" class="mono"></div>
          <div>Host state</div><div id="hostStateText"></div>
          <div>Rooms / Viewers</div><div><span id="roomsText"></span> / <span id="viewersText"></span></div>
          <div>Generated</div><div id="generatedAtText"></div>
        </div>
        <div class="actions">
          <button id="copyViewerBtn" type="button">Copy Viewer URL</button>
          <button id="copyTokenBtn" type="button">Copy Token</button>
          <a class="btn" href="./share_card.html" target="_blank" rel="noopener">Open Share Card</a>
          <a class="btn" href="./desktop_self_check.html" target="_blank" rel="noopener">Open Desktop Self-Check</a>
          <a class="btn" href="./desktop_self_check.txt" target="_blank" rel="noopener">Open Self-Check TXT</a>
          <a class="btn" href="./share_bundle.json" target="_blank" rel="noopener">Open JSON</a>
          <a class="btn" href="./share_diagnostics.txt" target="_blank" rel="noopener">Open Diagnostics</a>
        </div>
      </div>
      <div class="card">
        <h2>Viewer QR</h2>
        <div id="qrMount" class="qr" aria-live="polite">Loading QR...</div>
        <div class="actions">
          <button id="downloadQrBtn" type="button">Download QR SVG</button>
          <button id="copyViewerBtn2" type="button">Copy Viewer URL</button>
        </div>
        <div class="small mono" id="viewerUrlText" style="margin-top:12px;"></div>
      </div>
    </section>

    <section class="grid3" style="margin-bottom:20px;">
      <article class="step" id="stepServer">
        <div class="title-row"><h3>1. Server</h3><span id="serverBadge" class="pill">pending</span></div>
        <div class="small" id="serverNote">The desktop host should keep the HTTPS/WSS server running while the viewer joins.</div>
      </article>
      <article class="step" id="stepHost">
        <div class="title-row"><h3>2. Host page</h3><span id="hostBadge" class="pill">pending</span></div>
        <div class="small" id="hostNote">Open the host page in the embedded browser, then start screen sharing from the button inside the host page.</div>
      </article>
      <article class="step" id="stepViewer">
        <div class="title-row"><h3>3. Viewer access</h3><span id="viewerBadge" class="pill">pending</span></div>
        <div class="small" id="viewerNote">The viewer can scan the QR or enter the Viewer URL manually. If the certificate is self-signed, trust it for this local session.</div>
      </article>
    </section>

    <section class="grid2" style="margin-bottom:20px;">
      <article class="card">
        <h2>Diagnostic self-check</h2>
        <div class="small">These cards now consume the exported <span class="mono">selfCheck.items</span> model first, so Share Wizard and desktop self-check stay aligned.</div>
        <div class="small" style="margin-top:10px;">Passed: <span id="checkSummaryText" class="em">0 / 0</span></div>
        <div class="summary-row">
          <span class="pill p0" id="wizP0Pill">P0: 0</span>
          <span class="pill p1" id="wizP1Pill">P1: 0</span>
          <span class="pill p2" id="wizP2Pill">P2: 0</span>
          <span class="pill certificate" id="wizCertPill">Certificate: 0</span>
          <span class="pill network" id="wizNetworkPill">Network: 0</span>
          <span class="pill sharing" id="wizSharingPill">Sharing: 0</span>
        </div>
        <div class="filter-row">
          <strong>Category</strong>
          <button class="filter active" type="button" data-filter-kind="category" data-filter-value="all">All</button>
          <button class="filter" type="button" data-filter-kind="category" data-filter-value="certificate">Certificate</button>
          <button class="filter" type="button" data-filter-kind="category" data-filter-value="network">Network</button>
          <button class="filter" type="button" data-filter-kind="category" data-filter-value="sharing">Sharing</button>
        </div>
        <div class="filter-row">
          <strong>Severity</strong>
          <button class="filter active" type="button" data-filter-kind="severity" data-filter-value="all">All</button>
          <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P0">P0</button>
          <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P1">P1</button>
          <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P2">P2</button>
        </div>
        <div class="small" id="diagFilterSummaryText" style="margin-top:10px;">Showing all checks.</div>
        <div class="diag-grid" id="diagGrid"></div>
      </article>
      <article class="card">
        <h2>Common failure scenarios</h2>
        <div class="small">The wizard keeps this list focused on the issues that match the current state instead of showing a long static FAQ.</div>
        <ul class="issues" id="failureList"></ul>
        <h3 style="margin-top:18px;">Suggested next actions</h3>
        <div class="small">Use these as the fastest operator workflow before exporting or sharing the bundle again.</div>
        <ul class="action-list" id="actionList"></ul>
      </article>
    </section>

    <section class="grid2">
      <article class="card">
        <h2>Connection paths</h2>
        <div class="box" style="margin-bottom:14px;">
          <h3 style="margin-bottom:8px;">Same LAN / Wi-Fi</h3>
          <ol class="small">
            <li>Join the same router or Wi-Fi as the host.</li>
            <li>Open the Viewer URL or scan the QR.</li>
            <li>Accept the self-signed certificate warning if the browser asks.</li>
          </ol>
        </div>
        <div class="box" style="margin-bottom:14px;">
          <h3 style="margin-bottom:8px;">Local Hotspot</h3>
          <div class="kv" style="margin-top:0;">
            <div>SSID</div><div id="ssidText" class="mono"></div>
            <div>Password</div><div id="pwdText" class="mono"></div>
            <div>Status</div><div id="hotspotStatusText"></div>
          </div>
          <ol class="small">
            <li>Start the hotspot from the desktop host or from Windows Mobile Hotspot settings.</li>
            <li>Connect the phone or another PC to that hotspot.</li>
            <li>Open the Viewer URL after the device has an IP on the same local link.</li>
          </ol>
        </div>
        <div class="box">
          <h3 style="margin-bottom:8px;">Wi-Fi Direct</h3>
          <div class="kv" style="margin-top:0;">
            <div>API</div><div id="wifiDirectApiText"></div>
            <div>Alias</div><div id="wifiDirectAliasText" class="mono"></div>
          </div>
          <ol class="small">
            <li>Open the Windows Wi-Fi Direct / device pairing flow from the desktop host.</li>
            <li>Use the alias above to identify the correct session.</li>
            <li>Keep the Viewer URL as the actual media entry even after pairing.</li>
          </ol>
        </div>
      </article>
      <article class="card">
        <h2>Certificate & bundle support</h2>
        <div class="box" style="margin-bottom:14px;">
          <div class="kv" style="margin-top:0;">
            <div>Cert dir</div><div id="certDirText" class="mono"></div>
            <div>Cert file</div><div id="certFileText" class="mono"></div>
            <div>Key file</div><div id="certKeyText" class="mono"></div>
            <div>Status</div><div id="certStateText"></div>
          </div>
          <div class="small" id="certHintText" style="margin-top:10px;"></div>
        </div>
        <div class="box">
          <h3 style="margin-bottom:8px;">Bundle files</h3>
          <div class="kv" style="margin-top:0;">
            <div class="mono">share_card.html / share_wizard.html</div>
            <div>Human-friendly handoff pages with live polling from <span class="mono">share_status.js</span>.</div>
            <div class="mono">share_bundle.json / share_status.js / desktop_self_check.html</div>
            <div>Machine-readable snapshot plus the script reloaded by already-open pages.</div>
            <div class="mono">share_diagnostics.txt / desktop_self_check.txt</div>
            <div>Support-oriented snapshot with self-check summary and targeted troubleshooting tips.</div>
            <div class="mono">viewer_url.txt / hotspot_credentials.txt / share_readme.txt</div>
            <div>Manual fallback material for cases where QR scanning or browser trust prompts slow things down.</div>
          </div>
        </div>
        <div class="actions">
          <button id="copySsidBtn" type="button">Copy SSID</button>
          <button id="copyPwdBtn" type="button">Copy Password</button>
          <a class="btn" href="./share_readme.txt" target="_blank" rel="noopener">Open README</a>
        </div>
      </article>
    </section>
  </main>

  <script id="bundleJson" type="application/json">)HTML";
    html << bundleJson;
    html << R"HTML(</script>
  <script src="./www/assets/share_card_qr.bundle.js"></script>
  <script>
    let bundle = JSON.parse(document.getElementById('bundleJson').textContent);
    let currentViewerUrl = '';
    const activeFilters = { category: 'all', severity: 'all' };

    function setText(id, text) {
      const el = document.getElementById(id);
      if (el) el.textContent = text == null ? '' : String(text);
    }

    function htmlEscape(text) {
      return String(text == null ? '' : text)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    }

    function normalizeSeverity(v) {
      return v === 'P0' || v === 'P1' ? v : 'P2';
    }

    function normalizeCategory(v) {
      return v === 'certificate' || v === 'network' ? v : 'sharing';
    }

    function pillClassForSeverity(v) {
      return normalizeSeverity(v).toLowerCase();
    }

    function pillClassForCategory(v) {
      return normalizeCategory(v);
    }

    function filterItems(items) {
      return (items || []).filter(item => {
        const categoryOk = activeFilters.category === 'all' || normalizeCategory(item.category) === activeFilters.category;
        const severityOk = activeFilters.severity === 'all' || normalizeSeverity(item.severity) === activeFilters.severity;
        return categoryOk && severityOk;
      });
    }

    function updateWizardSummaryPills(report) {
      const sev = report.severityCounts || {};
      const cat = report.categoryCounts || {};
      setText('wizP0Pill', 'P0: ' + (sev.P0 || 0));
      setText('wizP1Pill', 'P1: ' + (sev.P1 || 0));
      setText('wizP2Pill', 'P2: ' + (sev.P2 || 0));
      setText('wizCertPill', 'Certificate: ' + (cat.certificate || 0));
      setText('wizNetworkPill', 'Network: ' + (cat.network || 0));
      setText('wizSharingPill', 'Sharing: ' + (cat.sharing || 0));
    }

    function updateDiagFilterSummary(items) {
      const filtered = filterItems(items || []);
      const categoryText = activeFilters.category === 'all' ? 'all categories' : activeFilters.category;
      const severityText = activeFilters.severity === 'all' ? 'all severities' : activeFilters.severity;
      setText('diagFilterSummaryText', 'Showing ' + filtered.length + ' checks for ' + categoryText + ' / ' + severityText + '.');
    }

    function bindWizardFilters() {
      document.querySelectorAll('button.filter').forEach(btn => {
        btn.onclick = () => {
          const kind = btn.getAttribute('data-filter-kind');
          const value = btn.getAttribute('data-filter-value');
          activeFilters[kind] = value;
          document.querySelectorAll('button.filter[data-filter-kind="' + kind + '"]').forEach(x => x.classList.toggle('active', x === btn));
          renderChecks(bundle);
        };
      });
    }

    async function copyText(text, okLabel) {
      try {
        await navigator.clipboard.writeText(String(text || ''));
        alert(okLabel);
      } catch (_) {
        alert('Copy failed. Please copy it manually.');
      }
    }

    function updateSyncLabel(source) {
      const now = new Date();
      setText('lastSyncText', source + ' @ ' + now.toLocaleTimeString());
    }

    function downloadQr() {
      try {
        const svg = window.LanShareQr.buildSvgMarkup(currentViewerUrl, { cellSize: 8, margin: 4 });
        const blob = new Blob([svg], { type: 'image/svg+xml;charset=utf-8' });
        const href = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = href;
        a.download = 'viewer_qr.svg';
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(href), 1000);
      } catch (err) {
        alert('QR export failed: ' + err);
      }
    }

    function renderQr(url) {
      currentViewerUrl = String(url || '');
      const mount = document.getElementById('qrMount');
      try {
        if (!window.LanShareQr) throw new Error('Local QR bundle missing');
        mount.style.background = '#fff';
        mount.style.color = '#111827';
        window.LanShareQr.renderInto(mount, currentViewerUrl, { cellSize: 8, margin: 4 });
      } catch (err) {
        mount.style.background = '#111827';
        mount.style.color = '#edf2f7';
        mount.textContent = 'QR rendering failed. Use the Viewer URL manually.\n' + String(err);
      }
    }

    function setBadge(elOrId, status, label) {
      const el = typeof elOrId === 'string' ? document.getElementById(elOrId) : elOrId;
      if (!el) return;
      el.className = 'pill' + (status === 'ok' ? ' ok' : status === 'warn' ? ' warn' : '');
      el.textContent = label;
    }

    function setCardState(cardId, badgeId, status, note) {
      const card = document.getElementById(cardId);
      if (card) {
        card.classList.remove('ok', 'warn');
        if (status === 'ok' || status === 'warn') card.classList.add(status);
      }
      setBadge(badgeId, status, status === 'ok' ? 'ok' : status === 'warn' ? 'attention' : 'pending');
      const noteMap = { stepServer: 'serverNote', stepHost: 'hostNote', stepViewer: 'viewerNote' };
      const noteEl = document.getElementById(noteMap[cardId] || '');
      if (noteEl && note) noteEl.textContent = note;
    }

    function renderFailures(items) {
      const root = document.getElementById('failureList');
      if (!root) return;
      const filtered = filterItems((items || []).map(item => ({
        title: item.title,
        detail: item.detail,
        severity: normalizeSeverity(item.severity),
        category: normalizeCategory(item.category)
      })));
      if (!filtered.length) {
        root.innerHTML = '<li><strong>No matching issue in this filter view.</strong> Switch back to All to review the full troubleshooting list.</li>';
        return;
      }
      root.innerHTML = filtered.map(item => '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(item.detail) +
        ' <span class="pill ' + pillClassForSeverity(item.severity) + '">' + htmlEscape(item.severity) + '</span>' +
        ' <span class="pill ' + pillClassForCategory(item.category) + '">' + htmlEscape(item.category) + '</span></li>').join('');
    }

    function sortIssuesByPriority(items) {
      return (items || []).slice().sort((a, b) => {
        const sevOrder = { P0: 0, P1: 1, P2: 2 };
        const ak = sevOrder[normalizeSeverity(a.severity)] ?? 9;
        const bk = sevOrder[normalizeSeverity(b.severity)] ?? 9;
        if (ak !== bk) return ak - bk;
        const ac = normalizeCategory(a.category);
        const bc = normalizeCategory(b.category);
        if (ac !== bc) return ac.localeCompare(bc);
        return String(a.title || '').localeCompare(String(b.title || ''));
      });
    }

    function renderActionList(actions) {
      const root = document.getElementById('actionList');
      if (!root) return;
      if (!actions.length) {
        root.innerHTML = '<li><strong>No urgent action needed.</strong> You can share the bundle as-is, or use Re-run Checks from the desktop host after any state change.</li>';
        return;
      }
      root.innerHTML = actions.map(item => {
        const detail = item.action || item.detail || '';
        const secondary = item.action && item.detail && item.action !== item.detail
          ? '<div class="small" style="margin-top:6px;">Why: ' + htmlEscape(item.detail) + '</div>'
          : '';
        return '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(detail) +
          secondary +
          ' <span class="pill ' + pillClassForSeverity(item.severity) + '">' + htmlEscape(normalizeSeverity(item.severity)) + '</span>' +
          ' <span class="pill ' + pillClassForCategory(item.category) + '">' + htmlEscape(normalizeCategory(item.category)) + '</span></li>';
      }).join('');
    }
)HTML";
    html << R"HTML(

    function indexSelfCheckItems(report) {
      const map = Object.create(null);
      ((report && report.items) || []).forEach(item => { if (item && item.id) map[item.id] = item; });
      return map;
    }

    function getSelfCheckItem(map, id, title, ok, okDetail, warnDetail, severity, category) {
      if (map[id]) return map[id];
      return { id, title, ok: !!ok, status: ok ? 'ok' : 'attention', detail: ok ? okDetail : warnDetail, severity: normalizeSeverity(severity), category: normalizeCategory(category) };
    }

    function renderDiagGrid(items) {
      const root = document.getElementById('diagGrid');
      if (!root) return;
      const filtered = filterItems(items || []);
      if (!filtered.length) {
        root.innerHTML = '<div class="diag-item warn"><h3>No checks match the current filters</h3><div class="small">Clear one filter or switch back to All to review the full report.</div></div>';
        return;
      }
      root.innerHTML = filtered.map(item => {
        const cls = item.ok ? 'ok' : 'warn';
        const badge = htmlEscape(item.status || (item.ok ? 'ok' : 'attention'));
        const severity = normalizeSeverity(item.severity);
        const category = normalizeCategory(item.category);
        return '<article class="diag-item ' + cls + '">' +
          '<div class="checkline"><h3>' + htmlEscape(item.title) + '</h3><div style="display:flex; gap:8px; flex-wrap:wrap; justify-content:flex-end;">' +
          '<span class="pill ' + cls + '">' + badge + '</span>' +
          '<span class="pill ' + pillClassForSeverity(severity) + '">' + htmlEscape(severity) + '</span>' +
          '<span class="pill ' + pillClassForCategory(category) + '">' + htmlEscape(category) + '</span>' +
          '</div></div>' +
          '<div class="small">' + htmlEscape(item.detail) + '</div>' +
          '</article>';
      }).join('');
    }

    function buildSuggestedActions(data, report) {
      const issues = sortIssuesByPriority(Array.isArray(report.issues) ? report.issues.slice() : []);
      const actions = issues
        .filter(item => !!(item && (item.action || item.detail)))
        .map(item => ({
          title: item.title,
          detail: item.detail,
          action: item.action || item.detail,
          severity: normalizeSeverity(item.severity),
          category: normalizeCategory(item.category)
        }));
      if (actions.length) return actions.slice(0, 5);
      if ((data.viewers || 0) > 0) {
        return [{
          title: 'Viewer already connected',
          detail: 'A viewer is already attached. You can keep sharing, or still export a refreshed bundle for other devices.',
          action: 'Keep sharing, or export a refreshed bundle for any new device that still needs the latest Viewer URL and QR.',
          severity: 'P2',
          category: 'sharing'
        }];
      }
      return [{
        title: 'Healthy bundle',
        detail: 'The current exported bundle looks consistent. Share Wizard, Share Card, and Desktop Self-Check are aligned to the same self-check snapshot.',
        action: 'Share the current bundle as-is, or use Re-run Checks after any change to server, IP, hotspot, or sharing state.',
        severity: 'P2',
        category: 'sharing'
      }];
    }

    function renderChecks(data) {
      const cert = data.cert || {};
      const report = data.selfCheck || {};
      const map = indexSelfCheckItems(report);
      const serverItem = getSelfCheckItem(map, 'server-reachability', 'Server reachability', !!(data.checks && data.checks.serverReady), 'The desktop host reports the HTTPS/WSS server as running.', 'The desktop host is not reporting a running server. Press Start and wait for the host page to load.', 'P0', 'sharing');
      const hostItem = getSelfCheckItem(map, 'host-sharing-state', 'Host sharing state', String(data.hostState || '').toLowerCase() === 'sharing', 'Screen sharing is active in the embedded host page.', 'The host page is not sharing yet.', 'P1', 'sharing');
      const viewerItem = getSelfCheckItem(map, 'viewer-entry-url', 'Viewer entry URL', !!(data.links && data.links.viewerUrl), 'Viewer URL and QR target are present in the bundle.', 'Viewer URL is empty. Re-export after host IP and port are valid.', 'P1', 'sharing');
      const networkItem = getSelfCheckItem(map, 'host-network', 'Host network', !!data.hostIp && data.hostIp !== '(not found)' && data.hostIp !== '0.0.0.0', 'Host IPv4 looks usable.', 'Host IPv4 is missing. Refresh the IP, or start hotspot / join a LAN before exporting again.', 'P0', 'network');

      setCardState('stepServer', 'serverBadge', serverItem.ok ? 'ok' : 'warn', serverItem.detail);
      setCardState('stepHost', 'hostBadge', hostItem.ok ? 'ok' : 'warn', hostItem.detail);
      if ((data.viewers || 0) > 0) {
        setCardState('stepViewer', 'viewerBadge', 'ok', 'A viewer is already connected. Current viewers: ' + data.viewers + '.');
      } else if (!networkItem.ok) {
        setCardState('stepViewer', 'viewerBadge', 'warn', networkItem.detail);
      } else {
        setCardState('stepViewer', 'viewerBadge', viewerItem.ok ? 'warn' : 'warn', viewerItem.ok ? 'Viewer URL is ready. Scan the QR or open the URL manually on the other device.' : viewerItem.detail);
      }

      const preferredOrder = ['server-reachability', 'listen-port', 'server-health-endpoint', 'lan-bind-address', 'lan-entry-endpoint', 'adapter-selection', 'embedded-host-runtime', 'host-sharing-state', 'host-network', 'viewer-entry-url', 'certificate-files', 'wifi-adapter', 'hotspot-control', 'live-bundle-refresh'];
      const ordered = [];
      preferredOrder.forEach(id => { if (map[id]) ordered.push(map[id]); });
      ((report.items) || []).forEach(item => { if (item && !ordered.some(x => x.id === item.id)) ordered.push(item); });
      const summaryTotal = report.total || ordered.length || 0;
      const summaryPassed = report.passed || ordered.filter(item => item && item.ok).length;
      setText('checkSummaryText', summaryPassed + ' / ' + summaryTotal);
      updateWizardSummaryPills(report);
      updateDiagFilterSummary(ordered);
      renderDiagGrid(ordered);

      let failures = Array.isArray(report.issues) ? report.issues.slice() : [];
      if (!navigator.onLine) failures.push({ title: 'Browser reports offline mode', severity: 'P1', category: 'network', detail: 'The pages are local, but the viewer still needs local network reachability to the host IP. Disable airplane mode or reconnect to LAN / hotspot.' });
      renderFailures(failures);
      renderActionList(buildSuggestedActions(data, report));
    }

    function renderBundle(data, source) {
      if (!data || !data.links) return;
      bundle = data;
      const hotspot = data.hotspot || {};
      const wifiDirect = data.wifiDirect || {};
      const cert = data.cert || {};
      setText('modeText', data.networkMode || 'unknown');
      setText('hostIpText', data.hostIp || '(not found)');
      setText('portText', data.port || '');
      setText('roomText', data.room || '');
      setText('hostStateText', data.hostState || 'unknown');
      setText('roomsText', data.rooms || 0);
      setText('viewersText', data.viewers || 0);
      setText('generatedAtText', data.generatedAt || '');
      setText('viewerUrlText', (data.links && data.links.viewerUrl) || '');
      setText('ssidText', hotspot.ssid || '(not configured)');
      setText('pwdText', hotspot.password || '(not configured)');
      setText('hotspotStatusText', hotspot.running ? 'running' : 'stopped');
      setText('wifiDirectApiText', wifiDirect.apiAvailable ? 'available' : 'not detected');
      setText('wifiDirectAliasText', wifiDirect.sessionAlias || '(not set)');
      setText('certDirText', cert.dir || '(cert dir unknown)');
      setText('certFileText', cert.certFile || '(missing)');
      setText('certKeyText', cert.keyFile || '(missing)');
      setText('certStateText', cert.ready ? 'cert + key present' : 'missing or incomplete');
      setText('certHintText', cert.trustHint || 'If the browser warns about a self-signed certificate, trust it for this local session.');
      setText('liveStatusText', (data.bundle && data.bundle.statusScript) ? 'enabled' : 'disabled');
      updateSyncLabel(source || 'embedded snapshot');
      renderQr((data.links && data.links.viewerUrl) || '');
      renderChecks(data);
    }

    function loadLiveStatus() {
      const script = document.createElement('script');
      script.src = './share_status.js?ts=' + Date.now();
      script.async = true;
      script.onload = () => {
        if (window.__LAN_SHARE_STATUS__) renderBundle(window.__LAN_SHARE_STATUS__, 'share_status.js');
        script.remove();
      };
      script.onerror = () => {
        setText('lastSyncText', 'share_status.js unavailable');
        script.remove();
      };
      document.head.appendChild(script);
    }

    window.addEventListener('lan-share-status', (ev) => {
      if (ev && ev.detail) renderBundle(ev.detail, 'share_status.js event');
    });

    document.getElementById('copyViewerBtn').onclick = () => copyText((bundle.links && bundle.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('copyViewerBtn2').onclick = () => copyText((bundle.links && bundle.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('copyTokenBtn').onclick = () => copyText(bundle.token || '', 'Token copied');
    document.getElementById('copySsidBtn').onclick = () => copyText((bundle.hotspot && bundle.hotspot.ssid) || '', 'SSID copied');
    document.getElementById('copyPwdBtn').onclick = () => copyText((bundle.hotspot && bundle.hotspot.password) || '', 'Password copied');
    document.getElementById('downloadQrBtn').onclick = downloadQr;

    bindWizardFilters();
    renderBundle(bundle, 'embedded snapshot');
    window.setInterval(loadLiveStatus, bundle.refreshMs || 2000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) loadLiveStatus(); });
    window.addEventListener('focus', loadLiveStatus);
  </script>
</body>
</html>)HTML";
    return html.str();
}

static std::string BuildShareStatusJs(std::string_view bundleJson) {
    std::string js;
    js.reserve(bundleJson.size() + 256);
    js += "window.__LAN_SHARE_STATUS__ = ";
    js.append(bundleJson.begin(), bundleJson.end());
    js += ";\n";
    js += "window.__LAN_SHARE_STATUS_LOADED_AT__ = new Date().toISOString();\n";
    js += "try { window.dispatchEvent(new CustomEvent('lan-share-status', { detail: window.__LAN_SHARE_STATUS__ })); } catch (_) {}\n";
    return js;
}

static std::string BuildShareDiagnosticsText(std::wstring_view generatedAt,
                                             std::wstring_view mode,
                                             std::wstring_view hostIp,
                                             int port,
                                             std::wstring_view room,
                                             std::wstring_view hostState,
                                             std::size_t rooms,
                                             std::size_t viewers,
                                             std::wstring_view hotspotStatus,
                                             std::wstring_view hotspotSsid,
                                             bool wifiAdapterPresent,
                                             bool hotspotSupported,
                                             bool wifiDirectApiAvailable,
                                             std::wstring_view hostUrl,
                                             std::wstring_view viewerUrl,
                                             bool serverProcessRunning,
                                             bool portReady,
                                             std::wstring_view portDetail,
                                             bool localHealthReady,
                                             std::wstring_view localHealthDetail,
                                             bool hostIpReachable,
                                             std::wstring_view hostIpReachableDetail,
                                             bool lanBindReady,
                                             std::wstring_view lanBindDetail,
                                             int activeIpv4Candidates,
                                             bool selectedIpRecommended,
                                             std::wstring_view adapterHint,
                                             bool embeddedHostReady,
                                             std::wstring_view embeddedHostStatus,
                                             std::wstring_view certDir,
                                             std::wstring_view certFile,
                                             std::wstring_view certKeyFile,
                                             bool certFileExists,
                                             bool certKeyExists) {
    const auto report = BuildSelfCheckReport(hostState, hostIp, viewerUrl, viewers, serverProcessRunning,
                                             certFileExists, certKeyExists, portReady, portDetail,
                                             localHealthReady, localHealthDetail, hostIpReachable, hostIpReachableDetail,
                                             lanBindReady, lanBindDetail, activeIpv4Candidates, selectedIpRecommended, adapterHint,
                                             embeddedHostReady, embeddedHostStatus,
                                             wifiAdapterPresent, hotspotSupported, wifiDirectApiAvailable,
                                             hotspotStatus == L"running", true);

    std::ostringstream out;
    out << "LAN Screen Share diagnostics\r\n";
    out << "============================\r\n\r\n";
    out << "Generated: " << urlutil::WideToUtf8(std::wstring(generatedAt)) << "\r\n";
    out << "Mode: " << urlutil::WideToUtf8(std::wstring(mode)) << "\r\n";
    out << "Host IP: " << urlutil::WideToUtf8(std::wstring(hostIp)) << "\r\n";
    out << "Port: " << port << "\r\n";
    out << "Room: " << urlutil::WideToUtf8(std::wstring(room)) << "\r\n";
    out << "Host State: " << urlutil::WideToUtf8(std::wstring(hostState)) << "\r\n";
    out << "Rooms / Viewers: " << rooms << " / " << viewers << "\r\n\r\n";
    out << "Hotspot: " << urlutil::WideToUtf8(std::wstring(hotspotStatus)) << "\r\n";
    out << "Hotspot SSID: " << urlutil::WideToUtf8(std::wstring(hotspotSsid)) << "\r\n";
    out << "Wi-Fi adapter present: " << (wifiAdapterPresent ? "yes" : "no") << "\r\n";
    out << "Hotspot supported: " << (hotspotSupported ? "yes" : "no") << "\r\n";
    out << "Wi-Fi Direct API: " << (wifiDirectApiAvailable ? "available" : "not detected") << "\r\n\r\n";
    out << "Certificate files\r\n";
    out << "-----------------\r\n";
    out << "Cert dir: " << urlutil::WideToUtf8(std::wstring(certDir)) << "\r\n";
    out << "Cert file: " << urlutil::WideToUtf8(std::wstring(certFile)) << " (" << (certFileExists ? "present" : "missing") << ")\r\n";
    out << "Key file: " << urlutil::WideToUtf8(std::wstring(certKeyFile)) << " (" << (certKeyExists ? "present" : "missing") << ")\r\n";
    out << "Trust note: If the browser warns about a self-signed certificate, trust it for this local session.\r\n\r\n";
    out << "Host URL: " << urlutil::WideToUtf8(std::wstring(hostUrl)) << "\r\n";
    out << "Viewer URL: " << urlutil::WideToUtf8(std::wstring(viewerUrl)) << "\r\n\r\n";
    out << "Runtime checks\r\n";
    out << "--------------\r\n";
    out << "Server process running: " << (serverProcessRunning ? "yes" : "no") << "\r\n";
    out << "Listen port ready: " << (portReady ? "yes" : "no") << "\r\n";
    out << "Port detail: " << urlutil::WideToUtf8(std::wstring(portDetail)) << "\r\n";
    out << "Local /health ready: " << (localHealthReady ? "yes" : "no") << "\r\n";
    out << "Health detail: " << urlutil::WideToUtf8(std::wstring(localHealthDetail)) << "\r\n";
    out << "LAN bind ready: " << (lanBindReady ? "yes" : "no") << "\r\n";
    out << "Bind detail: " << urlutil::WideToUtf8(std::wstring(lanBindDetail)) << "\r\n";
    out << "Selected host IP reachable: " << (hostIpReachable ? "yes" : "no") << "\r\n";
    out << "Host IP reachability detail: " << urlutil::WideToUtf8(std::wstring(hostIpReachableDetail)) << "\r\n";
    out << "Active IPv4 candidates: " << activeIpv4Candidates << "\r\n";
    out << "Selected IP recommended: " << (selectedIpRecommended ? "yes" : "no") << "\r\n";
    out << "Adapter hint: " << urlutil::WideToUtf8(std::wstring(adapterHint)) << "\r\n";
    out << "Embedded host ready: " << (embeddedHostReady ? "yes" : "no") << "\r\n";
    out << "Embedded host status: " << urlutil::WideToUtf8(std::wstring(embeddedHostStatus)) << "\r\n\r\n";
    out << "Self-check summary\r\n";
    out << "-------------------\r\n";
    out << "Passed: " << report.passed << " / " << report.total << "\r\n";
    out << "Severity: P0=" << report.p0 << ", P1=" << report.p1 << ", P2=" << report.p2 << "\r\n";
    out << "Categories: certificate=" << report.certificateCount << ", network=" << report.networkCount << ", sharing=" << report.sharingCount << "\r\n";
    for (const auto& item : report.items) {
        out << "- [" << item.severity << "][" << item.category << "] " << item.title << ": " << item.status << "\r\n";
        out << "  " << item.detail << "\r\n";
    }
    out << "\r\nCommon failure scenarios\r\n";
    out << "------------------------\r\n";
    if (report.failures.empty()) {
        out << "- No obvious blocking issue detected.\r\n";
    } else {
        for (const auto& issue : report.failures) {
            out << "- [" << issue.severity << "][" << issue.category << "] " << issue.title << ": " << issue.detail << "\r\n";
        }
    }
    out << "\r\nOperator first actions\r\n";
    out << "----------------------\r\n";
    const auto actions = BuildOperatorFirstActions(report, 3);
    if (actions.empty()) {
        out << "- No urgent first action right now. Share the current bundle or re-run checks after any state change.\r\n";
    } else {
        for (const auto& action : actions) {
            out << "- [" << action.severity << "][" << action.category << "] " << action.title << ": "
                << (action.action.empty() ? action.detail : action.action) << "\r\n";
        }
    }
    out << "\r\nLive bundle notes\r\n";
    out << "-----------------\r\n";
    out << "- share_card.html and share_wizard.html poll share_status.js every ~2 seconds.\r\n";
    out << "- desktop_self_check.html uses the same exported state snapshot and also auto-refreshes from share_status.js.\r\n";
    out << "- share_bundle.json is the machine-readable snapshot for this export.\r\n";
    out << "- If an already-open page looks stale, refocus the page or reopen it from the bundle folder.\r\n";
    return out.str();
}

static std::string BuildShareReadmeText(std::wstring_view hostUrl,
                                        std::wstring_view viewerUrl,
                                        std::wstring_view hotspotSsid,
                                        std::wstring_view hotspotPassword,
                                        bool hotspotRunning,
                                        std::wstring_view wifiDirectAlias) {
    std::ostringstream out;
    out << "LAN Screen Share bundle\r\n";
    out << "========================\r\n\r\n";
    out << "Viewer URL:\r\n" << urlutil::WideToUtf8(std::wstring(viewerUrl)) << "\r\n\r\n";
    out << "Host URL:\r\n" << urlutil::WideToUtf8(std::wstring(hostUrl)) << "\r\n\r\n";
    out << "Hotspot SSID: " << urlutil::WideToUtf8(std::wstring(hotspotSsid.empty() ? L"(not configured)" : hotspotSsid)) << "\r\n";
    out << "Hotspot Password: " << urlutil::WideToUtf8(std::wstring(hotspotPassword.empty() ? L"(not configured)" : hotspotPassword)) << "\r\n";
    out << "Hotspot Status: " << (hotspotRunning ? "running" : "stopped") << "\r\n";
    out << "Wi-Fi Direct Alias: " << urlutil::WideToUtf8(std::wstring(wifiDirectAlias)) << "\r\n\r\n";
    out << "Recommended flows:\r\n";
    out << "1. Same LAN/Wi-Fi: connect both devices to the same local network, then open the Viewer URL.\r\n";
    out << "2. Hotspot: start the local hotspot first, join the hotspot from the other device, then open the Viewer URL.\r\n";
    out << "3. Wi-Fi Direct: complete Windows pairing first, use the alias above to identify the target session, then keep the Viewer URL as the media entry.\r\n\r\n";
    out << "Certificate note:\r\n";
    out << "- If the browser warns about a self-signed certificate, trust it for this local session.\r\n\r\n";
    out << "Live bundle behavior:\r\n";
    out << "- share_card.html, share_wizard.html, and desktop_self_check.html auto-refresh from share_status.js while they stay open.\r\n";
    out << "- share_diagnostics.txt records the latest exported state snapshot for support.\r\n";
    out << "- desktop_self_check.txt is the plain-text version of the desktop operator report.\r\n\r\n";
    return out.str();
}

} // namespace

MainWindow::MainWindow() {
}

MainWindow::~MainWindow() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool MainWindow::Create() {
    WNDCLASS wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    m_hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"LAN Screen Share Host (Win32 + WebView2)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1420,
        940,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this);

    if (!m_hwnd) return false;
    return true;
}

void MainWindow::Show() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void MainWindow::Hide() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_HIDE);
}

void MainWindow::OnCreate() {
    m_adminBackend = std::make_unique<AdminBackend>();
    m_server = std::make_unique<ServerController>();

    m_server->SetLogCallback([this](const std::wstring& line) {
        if (!m_hwnd) return;
        auto* heap = new std::wstring(line);
        PostMessageW(m_hwnd, WM_APP_LOG, (WPARAM)heap, 0);
    });

    if (m_adminBackend) {
        AdminBackend::Handlers handlers;
        handlers.refreshNetwork = [this]() {
            RefreshHostIp();
            RefreshNetworkCapabilities();
            RefreshHotspotState();
        };
        handlers.generateRoomToken = [this]() {
            GenerateRoomToken();
        };
        handlers.startServer = [this]() {
            StartServer();
        };
        handlers.stopServer = [this]() {
            StopServer();
        };
        handlers.startServiceOnly = [this]() {
            StartServiceOnly();
        };
        handlers.startAndOpenHost = [this]() {
            StartAndOpenHost();
        };
        handlers.openHost = [this]() {
            OpenHostPage();
        };
        handlers.openViewer = [this]() {
            OpenViewerPage();
        };
        handlers.copyHostUrl = [this]() {
            CopyHostUrl();
        };
        handlers.copyViewerUrl = [this]() {
            CopyViewerUrl();
        };
        handlers.exportBundle = [this]() {
            ExportShareBundle();
        };
        handlers.openOutput = [this]() {
            OpenOutputFolder();
        };
        handlers.openReport = [this]() {
            OpenDiagnosticsReport();
        };
        handlers.refreshBundle = [this]() {
            RefreshDiagnosticsBundle();
        };
        handlers.showShareWizard = [this]() {
            ShowShareWizard();
        };
        handlers.selectNetworkCandidate = [this](std::size_t index) {
            SelectNetworkCandidate(index);
        };
        handlers.startHotspot = [this]() {
            StartHotspot();
        };
        handlers.stopHotspot = [this]() {
            StopHotspot();
        };
        handlers.autoHotspot = [this]() {
            EnsureHotspotDefaults();
            RefreshNetworkPage();
            RefreshHtmlAdminPreview();
        };
        handlers.openHotspotSettings = [this]() {
            OpenSystemHotspotSettings();
        };
        handlers.openConnectedDevices = [this]() {
            OpenWifiDirectPairing();
        };
        handlers.navigatePage = [this](std::wstring page) {
            TrySetPageFromAdminTab(page);
        };
        handlers.applySessionConfig = [this](std::wstring room, std::wstring token, std::wstring bind, int port) {
            ApplySessionConfigFromAdmin(std::move(room), std::move(token), std::move(bind), port);
        };
        handlers.applyHotspotConfig = [this](std::wstring ssid, std::wstring password) {
            ApplyHotspotConfigFromAdmin(std::move(ssid), std::move(password));
        };
        m_adminBackend->SetHandlers(std::move(handlers));
    }

    EnsureHotspotDefaults();
    GenerateRoomToken();
    m_defaultServerExePath = (AppDir() / L"lan_screenshare_server.exe").wstring();
    m_defaultWwwPath = (AppDir() / L"www").wstring();
    m_defaultCertDir = (AppDir() / L"cert").wstring();
    m_outputDir = (AppDir() / L"out").wstring();

    const int LEFT_W = 720;
    const int PAD = 10;
    const int ROW_H = 22;
    const int BTN_H = 28;
    const int LABEL_W = 95;

    int x = PAD;
    int y = PAD;

    m_navDashboardBtn = CreateWindowW(L"BUTTON", L"Dashboard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_DASHBOARD, GetModuleHandle(nullptr), nullptr);
    m_navSetupBtn = CreateWindowW(L"BUTTON", L"Setup", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 140, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_SETUP, GetModuleHandle(nullptr), nullptr);
    m_navNetworkBtn = CreateWindowW(L"BUTTON", L"Network", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 280, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_NETWORK, GetModuleHandle(nullptr), nullptr);
    m_navSharingBtn = CreateWindowW(L"BUTTON", L"Sharing", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 420, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_SHARING, GetModuleHandle(nullptr), nullptr);
    m_navMonitorBtn = CreateWindowW(L"BUTTON", L"Monitor", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 560, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_MONITOR, GetModuleHandle(nullptr), nullptr);
    m_navDiagnosticsBtn = CreateWindowW(L"BUTTON", L"Diagnostics", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 700, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_DIAGNOSTICS, GetModuleHandle(nullptr), nullptr);
    m_navSettingsBtn = CreateWindowW(L"BUTTON", L"Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 840, y, 130, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_NAV_SETTINGS, GetModuleHandle(nullptr), nullptr);
    y += 42;

    m_dashboardIntro = CreateWindowW(L"STATIC",
        L"Dashboard shows readiness first. Detailed knobs stay in Setup.",
        WS_CHILD | WS_VISIBLE,
        x, y, LEFT_W - PAD * 2, ROW_H,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 28;

    m_dashboardStatusCard = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, y, LEFT_W - 210 - PAD * 3, 150,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_dashboardPrimaryBtn = CreateWindowW(L"BUTTON", L"Start Sharing", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 180 - PAD, y + 4, 170, 34, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DASH_START, GetModuleHandle(nullptr), nullptr);
    m_dashboardContinueBtn = CreateWindowW(L"BUTTON", L"Continue Setup", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 180 - PAD, y + 48, 170, 34, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DASH_CONTINUE, GetModuleHandle(nullptr), nullptr);
    m_dashboardWizardBtn = CreateWindowW(L"BUTTON", L"Open Wizard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 180 - PAD, y + 92, 170, 34, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DASH_WIZARD, GetModuleHandle(nullptr), nullptr);
    y += 162;

    const int cardGap = 10;
    const int cardW = (LEFT_W - PAD * 2 - cardGap) / 2;
    const int cardH = 112;
    m_dashboardNetworkCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        x, y, cardW, cardH, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_dashboardServiceCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        x + cardW + cardGap, y, cardW, cardH, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += cardH + cardGap;
    m_dashboardShareCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        x, y, cardW, cardH, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_dashboardHealthCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        x + cardW + cardGap, y, cardW, cardH, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += cardH + 18;

    m_dashboardSuggestionsLabel = CreateWindowW(L"STATIC", L"Recent Next Steps:", WS_CHILD | WS_VISIBLE,
        x, y, 160, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 24;

    const int suggestionTextW = LEFT_W - 310 - PAD * 2;
    const int suggestionBtnW = 92;
    for (int i = 0; i < 4; ++i) {
        const int rowY = y + i * 40;
        const int fixId = ID_BTN_DASH_SUGGESTION_FIX_1 + i * 10;
        const int infoId = ID_BTN_DASH_SUGGESTION_INFO_1 + i * 10;
        const int setupId = ID_BTN_DASH_SUGGESTION_SETUP_1 + i * 10;
        m_dashboardSuggestionText[i] = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            x, rowY + 8, suggestionTextW, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        m_dashboardSuggestionFixBtn[i] = CreateWindowW(L"BUTTON", L"Fix", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + suggestionTextW + 8, rowY + 2, suggestionBtnW, 28, m_hwnd, (HMENU)(INT_PTR)fixId, GetModuleHandle(nullptr), nullptr);
        m_dashboardSuggestionInfoBtn[i] = CreateWindowW(L"BUTTON", L"Info", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + suggestionTextW + 8 + suggestionBtnW + 6, rowY + 2, suggestionBtnW, 28, m_hwnd, (HMENU)(INT_PTR)infoId, GetModuleHandle(nullptr), nullptr);
        m_dashboardSuggestionSetupBtn[i] = CreateWindowW(L"BUTTON", L"Setup", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + suggestionTextW + 8 + (suggestionBtnW + 6) * 2, rowY + 2, suggestionBtnW, 28, m_hwnd, (HMENU)(INT_PTR)setupId, GetModuleHandle(nullptr), nullptr);
    }

    const int setupTop = PAD + 42;
    y = setupTop;
    m_setupTitle = CreateWindowW(L"STATIC", L"Session Setup", WS_CHILD | WS_VISIBLE,
        x, y, LEFT_W - PAD * 2, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 28;
    m_stepInfo = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"Use this page only for the current session: room, token, bind, port, start, and host preview.",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, y, LEFT_W - PAD * 2, 42,
        m_hwnd,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr);
    y += 54;

    m_sessionGroup = CreateWindowW(L"BUTTON", L"Module 1: Session Basics", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, LEFT_W - PAD * 2, 108, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 24;

    m_ipLabel = CreateWindowW(L"STATIC", L"Host IPv4:", WS_CHILD | WS_VISIBLE,
        x, y, LABEL_W, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_ipValue = CreateWindowW(L"STATIC", m_hostIp.c_str(), WS_CHILD | WS_VISIBLE,
        x + LABEL_W, y, 160, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_templateLabel = CreateWindowW(L"STATIC", L"Template:", WS_CHILD | WS_VISIBLE,
        x + 270, y, 70, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_templateCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x + 345, y - 3, 190, 200, m_hwnd, (HMENU)(INT_PTR)ID_COMBO_SESSION_TEMPLATE, GetModuleHandle(nullptr), nullptr);
    SendMessageW(m_templateCombo, CB_ADDSTRING, 0, (LPARAM)L"Quick Share");
    SendMessageW(m_templateCombo, CB_ADDSTRING, 0, (LPARAM)L"Fixed Room");
    SendMessageW(m_templateCombo, CB_ADDSTRING, 0, (LPARAM)L"Demo Mode");
    SendMessageW(m_templateCombo, CB_SETCURSEL, 0, 0);
    m_btnRefreshIp = CreateWindowW(L"BUTTON", L"Refresh IP", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 130 - PAD, y - 1, 120, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_REFRESH_IP, GetModuleHandle(nullptr), nullptr);
    y += 30;

    m_roomLabel = CreateWindowW(L"STATIC", L"Room:", WS_CHILD | WS_VISIBLE,
        x, y, LABEL_W, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_roomEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_room.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + LABEL_W, y - 2, 190, ROW_H + 4,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_tokenLabel = CreateWindowW(L"STATIC", L"Token:", WS_CHILD | WS_VISIBLE,
        x + 300, y, 50, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_tokenEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_token.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + 355, y - 2, 180, ROW_H + 4,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnGenerate = CreateWindowW(L"BUTTON", L"Auto Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 130 - PAD, y - 1, 120, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_GENERATE, GetModuleHandle(nullptr), nullptr);
    y += 42;

    m_serviceGroup = CreateWindowW(L"BUTTON", L"Module 2: Service Listen", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, LEFT_W - PAD * 2, 100, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 24;

    m_bindLabel = CreateWindowW(L"STATIC", L"Bind:", WS_CHILD | WS_VISIBLE,
        x, y, LABEL_W, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_bindEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_bindAddress.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + LABEL_W, y - 2, 190, ROW_H + 4,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_portLabel = CreateWindowW(L"STATIC", L"Port:", WS_CHILD | WS_VISIBLE,
        x + 300, y, 50, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    std::wstring portStr = std::to_wstring(m_port);
    m_portEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", portStr.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
        x + 355, y - 2, 120, ROW_H + 4,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_advancedToggle = CreateWindowW(L"BUTTON", L"Advanced", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 130 - PAD, y - 1, 120, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_ADVANCED, GetModuleHandle(nullptr), nullptr);
    y += 30;

    m_sanIpLabel = CreateWindowW(L"STATIC", L"SAN IP / Host IP:", WS_CHILD | WS_VISIBLE,
        x, y, 120, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_sanIpValue = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        x + 125, y, LEFT_W - 140 - PAD * 2, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 42;

    m_hotspotLabel = CreateWindowW(L"BUTTON", L"Module 3: Run Control", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, LEFT_W - PAD * 2, 78, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 24;

    m_btnStart = CreateWindowW(L"BUTTON", L"Start", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, 90, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_START, GetModuleHandle(nullptr), nullptr);
    m_btnStop = CreateWindowW(L"BUTTON", L"Stop", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 100, y, 90, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_STOP, GetModuleHandle(nullptr), nullptr);
    m_btnRestart = CreateWindowW(L"BUTTON", L"Restart", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 200, y, 90, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_RESTART, GetModuleHandle(nullptr), nullptr);
    m_btnServiceOnly = CreateWindowW(L"BUTTON", L"Service Only", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 300, y, 110, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_SERVICE_ONLY, GetModuleHandle(nullptr), nullptr);
    m_btnStartAndOpenHost = CreateWindowW(L"BUTTON", L"Start + Open Host", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 420, y, 160, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_START_AND_OPEN_HOST, GetModuleHandle(nullptr), nullptr);
    y += 42;

    m_shareInfoLabel = CreateWindowW(L"BUTTON", L"Module 4: Session Summary", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, y, LEFT_W - PAD * 2, 124, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 20;
    m_sessionSummaryLabel = CreateWindowW(L"STATIC", L"Live session summary before launch:", WS_CHILD | WS_VISIBLE,
        x, y, 240, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 20;
    m_sessionSummaryBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, y, LEFT_W - PAD * 2, 78,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    y += 88;

    m_hostPreviewPlaceholder = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        LEFT_W + PAD * 2, PAD, 1, 1, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnOpenHost = CreateWindowW(L"BUTTON", L"Open Host In Browser", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        LEFT_W + PAD * 2, PAD, 1, 1, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_HOST, GetModuleHandle(nullptr), nullptr);
    m_runtimeInfoCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        LEFT_W + PAD * 2, PAD, 1, 1, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    int ny = setupTop;
    m_networkTitle = CreateWindowW(L"STATIC", L"Network & Connectivity", WS_CHILD | WS_VISIBLE,
        x, ny, LEFT_W - PAD * 2, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    ny += 28;
    m_networkSummaryCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, ny, LEFT_W - 250 - PAD * 3, 110, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnRefreshNetwork = CreateWindowW(L"BUTTON", L"Re-Detect", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 220 - PAD, ny + 6, 210, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_REFRESH_NETWORK, GetModuleHandle(nullptr), nullptr);
    m_btnManualSelectIp = CreateWindowW(L"BUTTON", L"Manual Select Main IP", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 220 - PAD, ny + 46, 210, 30, m_hwnd, (HMENU)(INT_PTR)ID_BTN_MANUAL_SELECT_IP, GetModuleHandle(nullptr), nullptr);
    ny += 122;

    m_adapterListLabel = CreateWindowW(L"STATIC", L"Adapter Candidates", WS_CHILD | WS_VISIBLE,
        x, ny, 200, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    ny += 24;
    for (int i = 0; i < 4; ++i) {
        const int rowY = ny + i * 58;
        m_networkAdapterCards[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            x, rowY, LEFT_W - 170 - PAD * 3, 48, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        m_networkAdapterSelectBtns[i] = CreateWindowW(L"BUTTON", L"Use As Main", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x + LEFT_W - 160 - PAD, rowY + 9, 150, 28, m_hwnd, (HMENU)(INT_PTR)(ID_BTN_SELECT_ADAPTER_1 + i), GetModuleHandle(nullptr), nullptr);
    }
    ny += 4 * 58 + 8;

    m_hotspotGroup = CreateWindowW(L"BUTTON", L"Hotspot", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, ny, LEFT_W - PAD * 2, 148, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    ny += 24;
    m_hotspotStatusCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        x + 360, ny - 4, LEFT_W - 370 - PAD * 2, 86, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_hotspotSsidLabel = CreateWindowW(L"STATIC", L"SSID:", WS_CHILD | WS_VISIBLE,
        x, ny, LABEL_W, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_hotspotSsidEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_hotspotSsid.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + LABEL_W, ny - 2, 230, ROW_H + 4, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    ny += 30;
    m_hotspotPwdLabel = CreateWindowW(L"STATIC", L"Password:", WS_CHILD | WS_VISIBLE,
        x, ny, LABEL_W, ROW_H, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_hotspotPwdEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", m_hotspotPassword.c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + LABEL_W, ny - 2, 230, ROW_H + 4, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    ny += 34;
    m_btnAutoHotspot = CreateWindowW(L"BUTTON", L"Auto Generate", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, ny, 120, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_AUTO_HOTSPOT, GetModuleHandle(nullptr), nullptr);
    m_btnStartHotspot = CreateWindowW(L"BUTTON", L"Start Hotspot", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 130, ny, 120, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_START_HOTSPOT, GetModuleHandle(nullptr), nullptr);
    m_btnStopHotspot = CreateWindowW(L"BUTTON", L"Stop Hotspot", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 260, ny, 120, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_STOP_HOTSPOT, GetModuleHandle(nullptr), nullptr);
    m_btnOpenHotspotSettings = CreateWindowW(L"BUTTON", L"System Hotspot Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 390, ny, 180, BTN_H, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_HOTSPOT_SETTINGS, GetModuleHandle(nullptr), nullptr);
    ny += 42;

    m_wifiDirectGroup = CreateWindowW(L"BUTTON", L"Wi-Fi Direct / Pairing", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x, ny, LEFT_W - PAD * 2, 120, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    ny += 24;
    m_wifiDirectCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        x, ny, LEFT_W - 210 - PAD * 3, 74, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnPairWifiDirect = CreateWindowW(L"BUTTON", L"Open Connected Devices", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 190 - PAD, ny, 180, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_WIFI_DIRECT_PAIR, GetModuleHandle(nullptr), nullptr);
    m_btnOpenConnectedDevices = CreateWindowW(L"BUTTON", L"Open Settings Page", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 190 - PAD, ny + 34, 180, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_CONNECTED_DEVICES, GetModuleHandle(nullptr), nullptr);
    m_btnOpenPairingHelp = CreateWindowW(L"BUTTON", L"Pairing Help", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + LEFT_W - 190 - PAD, ny + 68, 180, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_PAIRING_HELP, GetModuleHandle(nullptr), nullptr);

    int sy = setupTop;
    m_sharingTitle = CreateWindowW(L"STATIC", L"Sharing Center", WS_CHILD | WS_VISIBLE,
        x, sy, LEFT_W - PAD * 2, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    sy += 28;
    m_accessEntryCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, sy, LEFT_W - PAD * 2, 164, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnCopyHostUrl = CreateWindowW(L"BUTTON", L"Copy Host URL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 14, sy + 112, 130, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_COPY_HOST_URL, GetModuleHandle(nullptr), nullptr);
    m_btnCopyViewerUrl = CreateWindowW(L"BUTTON", L"Copy Viewer URL", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 154, sy + 112, 140, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_COPY_VIEWER_URL_2, GetModuleHandle(nullptr), nullptr);
    m_btnOpenHostBrowser = CreateWindowW(L"BUTTON", L"Open Host In Browser", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 304, sy + 112, 160, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_HOST_BROWSER, GetModuleHandle(nullptr), nullptr);
    m_btnOpenViewerBrowser = CreateWindowW(L"BUTTON", L"Open Viewer In Browser", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 474, sy + 112, 170, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_VIEWER_BROWSER, GetModuleHandle(nullptr), nullptr);
    sy += 176;

    m_qrCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, sy, LEFT_W - PAD * 2, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnSaveQrImage = CreateWindowW(L"BUTTON", L"Save QR Image", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 14, sy + 124, 120, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_SAVE_QR_IMAGE, GetModuleHandle(nullptr), nullptr);
    m_btnFullscreenQr = CreateWindowW(L"BUTTON", L"Fullscreen QR", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 144, sy + 124, 120, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_FULLSCREEN_QR, GetModuleHandle(nullptr), nullptr);
    m_btnOpenShareCard = CreateWindowW(L"BUTTON", L"Open Share Card", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 274, sy + 124, 130, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_SHARE_CARD_2, GetModuleHandle(nullptr), nullptr);
    m_btnOpenShareWizard = CreateWindowW(L"BUTTON", L"Open Share Wizard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 414, sy + 124, 140, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_SHARE_WIZARD_2, GetModuleHandle(nullptr), nullptr);
    m_btnOpenBundleFolder = CreateWindowW(L"BUTTON", L"Open Bundle Folder", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 564, sy + 124, 140, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_OPEN_BUNDLE_FOLDER_2, GetModuleHandle(nullptr), nullptr);
    sy += 188;

    m_accessGuideCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, sy, LEFT_W - PAD * 2, 190, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnExportOfflineZip = CreateWindowW(L"BUTTON", L"Export Offline Package", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 14, sy + 148, 180, 28, m_hwnd, (HMENU)(INT_PTR)ID_BTN_EXPORT_OFFLINE_ZIP, GetModuleHandle(nullptr), nullptr);

    int my = setupTop;
    m_monitorTitle = CreateWindowW(L"STATIC", L"Live Monitor", WS_CHILD | WS_VISIBLE,
        x, my, LEFT_W - PAD * 2, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    my += 30;
    const int monitorGap = 8;
    const int metricW = (LEFT_W - PAD * 2 - monitorGap * 4) / 5;
    for (int i = 0; i < 5; ++i) {
        m_monitorMetricCards[i] = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            x + i * (metricW + monitorGap), my, metricW, 70, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    }
    my += 82;
    m_monitorTimelineLabel = CreateWindowW(L"STATIC", L"Session Timeline", WS_CHILD | WS_VISIBLE,
        x, my, 200, 22, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    my += 24;
    m_monitorTimelineBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, my, LEFT_W - PAD * 2, 150, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    my += 162;
    m_monitorTabHealth = CreateWindowW(L"BUTTON", L"Health Checks", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, my, 130, 28, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_monitorTabConnections = CreateWindowW(L"BUTTON", L"Connection Events", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 140, my, 150, 28, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_monitorTabLogs = CreateWindowW(L"BUTTON", L"Realtime Logs", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 300, my, 130, 28, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    my += 36;
    m_monitorDetailBox = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, my, LEFT_W - PAD * 2, 260, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    int dy = setupTop;
    m_diagPageTitle = CreateWindowW(L"STATIC", L"Diagnostics & Export", WS_CHILD | WS_VISIBLE,
        x, dy, LEFT_W - PAD * 2, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    dy += 28;
    m_diagChecklistCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, dy, 230, 180, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_diagActionsCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x + 240, dy, 230, 180, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_diagExportCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x + 480, dy, 220, 180, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    dy += 192;
    m_diagFilesCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, dy, LEFT_W - PAD * 2, 116, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    dy += 128;
    m_diagLogSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, dy, 200, 24, m_hwnd, (HMENU)(INT_PTR)ID_EDIT_DIAG_LOG_SEARCH, GetModuleHandle(nullptr), nullptr);
    m_diagLevelFilter = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x + 210, dy - 2, 120, 160, m_hwnd, (HMENU)(INT_PTR)ID_COMBO_DIAG_LEVEL, GetModuleHandle(nullptr), nullptr);
    SendMessageW(m_diagLevelFilter, CB_ADDSTRING, 0, (LPARAM)L"All Levels");
    SendMessageW(m_diagLevelFilter, CB_ADDSTRING, 0, (LPARAM)L"Info");
    SendMessageW(m_diagLevelFilter, CB_ADDSTRING, 0, (LPARAM)L"Warning");
    SendMessageW(m_diagLevelFilter, CB_ADDSTRING, 0, (LPARAM)L"Error");
    SendMessageW(m_diagLevelFilter, CB_SETCURSEL, 0, 0);
    m_diagSourceFilter = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x + 340, dy - 2, 140, 160, m_hwnd, (HMENU)(INT_PTR)ID_COMBO_DIAG_SOURCE, GetModuleHandle(nullptr), nullptr);
    SendMessageW(m_diagSourceFilter, CB_ADDSTRING, 0, (LPARAM)L"All Sources");
    SendMessageW(m_diagSourceFilter, CB_ADDSTRING, 0, (LPARAM)L"app");
    SendMessageW(m_diagSourceFilter, CB_ADDSTRING, 0, (LPARAM)L"network");
    SendMessageW(m_diagSourceFilter, CB_ADDSTRING, 0, (LPARAM)L"server");
    SendMessageW(m_diagSourceFilter, CB_ADDSTRING, 0, (LPARAM)L"webview");
    SendMessageW(m_diagSourceFilter, CB_SETCURSEL, 0, 0);
    m_btnDiagCopyLogs = CreateWindowW(L"BUTTON", L"Copy Logs", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 490, dy - 1, 100, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_COPY_LOGS, GetModuleHandle(nullptr), nullptr);
    m_btnDiagSaveLogs = CreateWindowW(L"BUTTON", L"Save Logs", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 600, dy - 1, 100, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_SAVE_LOGS, GetModuleHandle(nullptr), nullptr);
    dy += 34;
    m_diagLogViewer = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, dy, LEFT_W - PAD * 2, 224, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_btnDiagOpenOutput = CreateWindowW(L"BUTTON", L"Open Output", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 490, setupTop + 132, 100, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_OPEN_OUTPUT, GetModuleHandle(nullptr), nullptr);
    m_btnDiagOpenReport = CreateWindowW(L"BUTTON", L"Open Report", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 600, setupTop + 132, 100, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_OPEN_REPORT, GetModuleHandle(nullptr), nullptr);
    m_btnDiagExportZip = CreateWindowW(L"BUTTON", L"Export Zip", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 490, setupTop + 162, 100, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_EXPORT_ZIP, GetModuleHandle(nullptr), nullptr);
    m_btnDiagCopyPath = CreateWindowW(L"BUTTON", L"Copy Path", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 600, setupTop + 162, 100, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_COPY_PATH, GetModuleHandle(nullptr), nullptr);
    m_btnDiagRefreshBundle = CreateWindowW(L"BUTTON", L"Refresh Bundle", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 545, setupTop + 102, 120, 26, m_hwnd, (HMENU)(INT_PTR)ID_BTN_DIAG_REFRESH_BUNDLE, GetModuleHandle(nullptr), nullptr);

    int gy = setupTop;
    m_settingsTitle = CreateWindowW(L"STATIC", L"Settings", WS_CHILD | WS_VISIBLE,
        x, gy, LEFT_W - PAD * 2, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    gy += 28;
    m_settingsIntro = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, gy, LEFT_W - PAD * 2, 56, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    gy += 68;
    m_settingsGeneralCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, gy, 226, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_settingsServiceCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x + 236, gy, 226, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_settingsNetworkCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x + 472, gy, 228, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    gy += 188;
    m_settingsSharingCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, gy, 226, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_settingsLoggingCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x + 236, gy, 226, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    m_settingsAdvancedCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x + 472, gy, 228, 176, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    gy += 188;
    m_settingsCurrentStateCard = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        x, gy, LEFT_W - PAD * 2, 170, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    RECT right{};
    right.left = LEFT_W + PAD;
    right.top = 0;
    right.right = rc.right;
    right.bottom = rc.bottom;

    m_webview.Initialize(
        m_hwnd,
        right,
        [this](std::wstring msg) {
            auto* heap = new std::wstring(std::move(msg));
            PostMessageW(m_hwnd, WM_APP_LOG, (WPARAM)heap, 0);
        },
        [this](std::wstring payload) {
            if (!m_hwnd) return;
            auto* heap = new std::wstring(std::move(payload));
            PostMessageW(m_hwnd, WM_APP_WEBVIEW, (WPARAM)heap, 0);
        });

    RefreshNetworkCapabilities();
    RefreshHotspotState();
    RefreshHostIp();

    SetTimer(m_hwnd, TIMER_ID, 1000, nullptr);

    AppendLog(L"UI created");
    RefreshShareInfo();
    RefreshDashboard();
    SetPage(UiPage::Dashboard);
    UpdateUiState();
}

void MainWindow::OnSize(int width, int height) {
    const int LEFT_W = 720;
    const int PAD = 10;
    const int rightLeft = LEFT_W + PAD;
    const int rightWidth = std::max(240, width - rightLeft - PAD);
    const int previewHeight = std::max(260, height - 220);
    const int runtimeTop = PAD + previewHeight + PAD;
    const int runtimeHeight = std::max(120, height - runtimeTop - PAD);

    RECT right{};
    right.left = rightLeft;
    right.top = 0;
    right.right = width;
    right.bottom = previewHeight;
    if (IsHtmlAdminActive()) {
        RECT admin{};
        admin.left = PAD;
        admin.top = 52;
        admin.right = width - PAD;
        admin.bottom = height - PAD;
        m_webview.Resize(admin);
    } else if (m_currentPage == UiPage::Setup && m_webviewMode == WebViewSurfaceMode::HostPreview) {
        m_webview.Resize(right);
    } else {
        RECT hidden{};
        hidden.left = rightLeft;
        hidden.top = 0;
        hidden.right = rightLeft;
        hidden.bottom = 0;
        m_webview.Resize(hidden);
    }

    if (m_hostPreviewPlaceholder) {
        MoveWindow(m_hostPreviewPlaceholder, rightLeft + PAD, PAD, rightWidth - PAD, previewHeight - PAD, TRUE);
    }
    if (m_btnOpenHost) {
        MoveWindow(m_btnOpenHost, rightLeft + PAD + 16, PAD + 84, 180, 30, TRUE);
    }
    if (m_runtimeInfoCard) {
        MoveWindow(m_runtimeInfoCard, rightLeft + PAD, runtimeTop, rightWidth - PAD, runtimeHeight, TRUE);
    }
}

void MainWindow::SetPage(UiPage page) {
    m_currentPage = page;
    if (PreferHtmlAdminUi()) {
        NavigateHtmlAdminInWebView();
    } else if (m_currentPage == UiPage::Setup && m_server && m_server->IsRunning()) {
        NavigateHostInWebView();
    } else {
        m_webviewMode = WebViewSurfaceMode::Hidden;
    }
    UpdatePageVisibility();
    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right, rc.bottom);
    RefreshDashboard();
    RefreshSessionSetup();
    RefreshNetworkPage();
    RefreshSharingPage();
    RefreshMonitorPage();
    RefreshDiagnosticsPage();
    RefreshSettingsPage();
    RefreshHtmlAdminPreview();
}

void MainWindow::UpdatePageVisibility() {
    const bool usingHtmlAdmin = IsHtmlAdminActive();
    const bool dashboard = m_currentPage == UiPage::Dashboard;
    const bool setup = m_currentPage == UiPage::Setup;
    const bool network = m_currentPage == UiPage::Network;
    const bool sharing = m_currentPage == UiPage::Sharing;
    const bool monitor = m_currentPage == UiPage::Monitor;
    const bool diagnostics = m_currentPage == UiPage::Diagnostics;
    const bool settings = m_currentPage == UiPage::Settings;
    const bool settingsNative = settings && !usingHtmlAdmin;
    const auto setMany = [](std::initializer_list<HWND> controls, bool visible) {
        for (HWND hwnd : controls) {
            if (hwnd) ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
        }
    };

    setMany({
        m_dashboardIntro,
        m_dashboardStatusCard,
        m_dashboardPrimaryBtn,
        m_dashboardContinueBtn,
        m_dashboardWizardBtn,
        m_dashboardNetworkCard,
        m_dashboardServiceCard,
        m_dashboardShareCard,
        m_dashboardHealthCard,
        m_dashboardSuggestionsLabel,
    }, dashboard && !usingHtmlAdmin);
    for (HWND hwnd : m_dashboardSuggestionText) ShowWindow(hwnd, (dashboard && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);
    for (HWND hwnd : m_dashboardSuggestionFixBtn) ShowWindow(hwnd, (dashboard && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);
    for (HWND hwnd : m_dashboardSuggestionInfoBtn) ShowWindow(hwnd, (dashboard && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);
    for (HWND hwnd : m_dashboardSuggestionSetupBtn) ShowWindow(hwnd, (dashboard && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);

    setMany({
        m_setupTitle,
        m_stepInfo,
        m_sessionGroup,
        m_serviceGroup,
        m_templateLabel,
        m_templateCombo,
        m_ipLabel,
        m_ipValue,
        m_btnRefreshIp,
        m_hotspotLabel,
        m_bindLabel,
        m_bindEdit,
        m_sanIpLabel,
        m_sanIpValue,
        m_advancedToggle,
        m_portLabel,
        m_portEdit,
        m_roomLabel,
        m_roomEdit,
        m_tokenLabel,
        m_tokenEdit,
        m_btnGenerate,
        m_btnStart,
        m_btnStop,
        m_btnRestart,
        m_btnServiceOnly,
        m_btnStartAndOpenHost,
        m_shareInfoLabel,
        m_sessionSummaryLabel,
        m_sessionSummaryBox,
        m_hostPreviewPlaceholder,
        m_btnOpenHost,
        m_runtimeInfoCard,
    }, setup && !usingHtmlAdmin);

    setMany({
        m_networkTitle,
        m_networkSummaryCard,
        m_btnRefreshNetwork,
        m_btnManualSelectIp,
        m_adapterListLabel,
        m_hotspotGroup,
        m_hotspotStatusCard,
        m_hotspotSsidLabel,
        m_hotspotPwdLabel,
        m_hotspotSsidEdit,
        m_hotspotPwdEdit,
        m_btnAutoHotspot,
        m_btnStartHotspot,
        m_btnStopHotspot,
        m_btnOpenHotspotSettings,
        m_wifiDirectGroup,
        m_wifiDirectCard,
        m_btnPairWifiDirect,
        m_btnOpenConnectedDevices,
        m_btnOpenPairingHelp,
    }, network && !usingHtmlAdmin);
    for (HWND hwnd : m_networkAdapterCards) ShowWindow(hwnd, (network && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);
    for (HWND hwnd : m_networkAdapterSelectBtns) ShowWindow(hwnd, (network && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);

    setMany({
        m_sharingTitle,
        m_accessEntryCard,
        m_qrCard,
        m_accessGuideCard,
        m_btnCopyHostUrl,
        m_btnCopyViewerUrl,
        m_btnOpenHostBrowser,
        m_btnOpenViewerBrowser,
        m_btnSaveQrImage,
        m_btnFullscreenQr,
        m_btnOpenShareCard,
        m_btnOpenShareWizard,
        m_btnOpenBundleFolder,
        m_btnExportOfflineZip,
    }, sharing && !usingHtmlAdmin);

    setMany({
        m_monitorTitle,
        m_monitorTimelineLabel,
        m_monitorTimelineBox,
        m_monitorTabHealth,
        m_monitorTabConnections,
        m_monitorTabLogs,
        m_monitorDetailBox,
    }, monitor && !usingHtmlAdmin);
    for (HWND hwnd : m_monitorMetricCards) ShowWindow(hwnd, (monitor && !usingHtmlAdmin) ? SW_SHOW : SW_HIDE);

    setMany({
        m_diagPageTitle,
        m_diagChecklistCard,
        m_diagActionsCard,
        m_diagExportCard,
        m_diagFilesCard,
        m_diagLogSearch,
        m_diagLevelFilter,
        m_diagSourceFilter,
        m_diagLogViewer,
        m_btnDiagOpenOutput,
        m_btnDiagOpenReport,
        m_btnDiagExportZip,
        m_btnDiagCopyPath,
        m_btnDiagRefreshBundle,
        m_btnDiagCopyLogs,
        m_btnDiagSaveLogs,
    }, diagnostics && !usingHtmlAdmin);

    setMany({
        m_settingsTitle,
        m_settingsIntro,
        m_settingsGeneralCard,
        m_settingsServiceCard,
        m_settingsNetworkCard,
        m_settingsSharingCard,
        m_settingsLoggingCard,
        m_settingsAdvancedCard,
        m_settingsCurrentStateCard,
    }, settingsNative);

    if (m_navDashboardBtn) EnableWindow(m_navDashboardBtn, dashboard ? FALSE : TRUE);
    if (m_navSetupBtn) EnableWindow(m_navSetupBtn, setup ? FALSE : TRUE);
    if (m_navNetworkBtn) EnableWindow(m_navNetworkBtn, network ? FALSE : TRUE);
    if (m_navSharingBtn) EnableWindow(m_navSharingBtn, sharing ? FALSE : TRUE);
    if (m_navMonitorBtn) EnableWindow(m_navMonitorBtn, monitor ? FALSE : TRUE);
    if (m_navDiagnosticsBtn) EnableWindow(m_navDiagnosticsBtn, diagnostics ? FALSE : TRUE);
    if (m_navSettingsBtn) EnableWindow(m_navSettingsBtn, settings ? FALSE : TRUE);
}

void MainWindow::ExecuteDashboardSuggestionFix(std::size_t index) {
    if (index >= m_dashboardSuggestionKinds.size()) return;
    switch (m_dashboardSuggestionKinds[index]) {
    case DashboardSuggestionKind::StartServer:
        StartServer();
        break;
    case DashboardSuggestionKind::OpenQuickWizard:
        ShowShareWizard();
        break;
    case DashboardSuggestionKind::OpenDiagnostics:
        OpenDiagnosticsReport();
        break;
    case DashboardSuggestionKind::OpenHostExternally:
        OpenHostPage();
        break;
    case DashboardSuggestionKind::StartHotspot:
        StartHotspot();
        break;
    case DashboardSuggestionKind::OpenHotspotSettings:
        OpenSystemHotspotSettings();
        break;
    case DashboardSuggestionKind::RefreshIp:
        RefreshHostIp();
        break;
    case DashboardSuggestionKind::NoteSelfSignedCert:
        OpenHostPage();
        break;
    case DashboardSuggestionKind::PortReady:
        RefreshDiagnosticsBundle();
        break;
    case DashboardSuggestionKind::None:
    default:
        break;
    }
}

void MainWindow::ExecuteDashboardSuggestionInfo(std::size_t index) {
    if (index >= m_dashboardSuggestionKinds.size()) return;
    if (!m_dashboardSuggestionDetails[index].empty()) {
        MessageBoxW(m_hwnd, m_dashboardSuggestionDetails[index].c_str(), L"Dashboard Hint", MB_OK | MB_ICONINFORMATION);
    }
    switch (m_dashboardSuggestionKinds[index]) {
    case DashboardSuggestionKind::OpenQuickWizard:
        ShowShareWizard();
        break;
    default:
        break;
    }
}

void MainWindow::AddTimelineEvent(std::wstring_view eventText) {
    m_timelineText += L"[" + NowTs() + L"] " + std::wstring(eventText) + L"\r\n";
    if (m_timelineText.size() > 24000) {
        m_timelineText.erase(0, m_timelineText.size() - 20000);
    }
}

void MainWindow::OnCommand(int id) {
    switch (id) {
    case ID_BTN_NAV_DASHBOARD:
        SetPage(UiPage::Dashboard);
        break;
    case ID_BTN_NAV_SETUP:
    case ID_BTN_DASH_CONTINUE:
        SetPage(UiPage::Setup);
        break;
    case ID_BTN_NAV_NETWORK:
        SetPage(UiPage::Network);
        break;
    case ID_BTN_NAV_SHARING:
        SetPage(UiPage::Sharing);
        break;
    case ID_BTN_NAV_MONITOR:
        SetPage(UiPage::Monitor);
        break;
    case ID_BTN_NAV_DIAGNOSTICS:
        SetPage(UiPage::Diagnostics);
        break;
    case ID_BTN_NAV_SETTINGS:
        SetPage(UiPage::Settings);
        break;
    case ID_EDIT_DIAG_LOG_SEARCH:
    case ID_COMBO_DIAG_LEVEL:
    case ID_COMBO_DIAG_SOURCE:
        RefreshFilteredLogs();
        break;
    case ID_BTN_DASH_START:
        StartServer();
        break;
    case ID_BTN_DASH_WIZARD:
        ShowShareWizard();
        break;
    case ID_COMBO_SESSION_TEMPLATE: {
        const int sel = m_templateCombo ? static_cast<int>(SendMessageW(m_templateCombo, CB_GETCURSEL, 0, 0)) : 0;
        if (sel == 0) {
            GenerateRoomToken();
        } else if (sel == 1) {
            m_room = L"meeting-room";
            m_token = L"persistent-token";
            if (m_roomEdit) SetWindowTextW(m_roomEdit, m_room.c_str());
            if (m_tokenEdit) SetWindowTextW(m_tokenEdit, m_token.c_str());
            RefreshShareInfo();
        } else if (sel == 2) {
            m_room = L"demo";
            m_token = L"demo-view";
            if (m_roomEdit) SetWindowTextW(m_roomEdit, m_room.c_str());
            if (m_tokenEdit) SetWindowTextW(m_tokenEdit, m_token.c_str());
            RefreshShareInfo();
        }
        break;
    }
    case ID_BTN_REFRESH_IP:
        RefreshHostIp();
        break;
    case ID_BTN_REFRESH_NETWORK:
        RefreshHostIp();
        RefreshNetworkCapabilities();
        RefreshHotspotState();
        break;
    case ID_BTN_MANUAL_SELECT_IP:
        SetPage(UiPage::Network);
        break;
    case ID_BTN_AUTO_HOTSPOT:
        EnsureHotspotDefaults();
        RefreshNetworkPage();
        break;
    case ID_BTN_GENERATE:
        GenerateRoomToken();
        break;
    case ID_BTN_START:
        StartServer();
        break;
    case ID_BTN_STOP:
        StopServer();
        break;
    case ID_BTN_RESTART:
        RestartServer();
        break;
    case ID_BTN_SERVICE_ONLY:
        StartServiceOnly();
        break;
    case ID_BTN_START_AND_OPEN_HOST:
        StartAndOpenHost();
        break;
    case ID_BTN_COPY_HOST_URL: {
        CopyHostUrl();
        break;
    }
    case ID_BTN_OPEN_HOST:
    case ID_BTN_OPEN_HOST_BROWSER:
        OpenHostPage();
        break;
    case ID_BTN_OPEN_VIEWER:
    case ID_BTN_OPEN_VIEWER_BROWSER:
        OpenViewerPage();
        break;
    case ID_BTN_COPY_VIEWER:
    case ID_BTN_COPY_VIEWER_URL_2:
        CopyViewerUrl();
        break;
    case ID_BTN_SHOW_QR:
    case ID_BTN_FULLSCREEN_QR:
    case ID_BTN_OPEN_SHARE_CARD_2:
        ShowQr();
        break;
    case ID_BTN_SHOW_WIZARD:
    case ID_BTN_OPEN_SHARE_WIZARD_2:
        ShowShareWizard();
        break;
    case ID_BTN_EXPORT_BUNDLE:
    case ID_BTN_EXPORT_OFFLINE_ZIP:
        ExportShareBundle();
        break;
    case ID_BTN_OPEN_BUNDLE_FOLDER_2:
        OpenOutputFolder();
        break;
    case ID_BTN_SAVE_QR_IMAGE:
        ExportShareBundle();
        AppendLog(L"Exported sharing bundle for QR save");
        break;
    case ID_BTN_DIAG_OPEN_OUTPUT:
        OpenOutputFolder();
        break;
    case ID_BTN_DIAG_OPEN_REPORT:
        OpenDiagnosticsReport();
        break;
    case ID_BTN_DIAG_EXPORT_ZIP:
        ExportShareBundle();
        AppendLog(L"Offline zip export placeholder: bundle refreshed");
        break;
    case ID_BTN_DIAG_COPY_PATH: {
        const auto path = (AppDir() / L"out" / L"share_bundle").wstring();
        if (urlutil::SetClipboardText(m_hwnd, path)) AppendLog(L"Diagnostics path copied");
        break;
    }
    case ID_BTN_DIAG_REFRESH_BUNDLE:
        RefreshDiagnosticsBundle();
        break;
    case ID_BTN_DIAG_COPY_LOGS:
        if (m_diagLogViewer) {
            wchar_t buf[32768]{};
            GetWindowTextW(m_diagLogViewer, buf, _countof(buf));
            urlutil::SetClipboardText(m_hwnd, buf);
        }
        break;
    case ID_BTN_DIAG_SAVE_LOGS:
        ExportShareBundle();
        AppendLog(L"Saved logs via bundle refresh placeholder");
        break;
    case ID_BTN_RUN_SELF_CHECK:
        RunDesktopSelfCheck();
        break;
    case ID_BTN_OPEN_DIAGNOSTICS:
        OpenDiagnosticsReport();
        break;
    case ID_BTN_REFRESH_CHECKS:
        RefreshDiagnosticsBundle();
        break;
    case ID_BTN_OPEN_FOLDER:
        OpenOutputFolder();
        break;
    case ID_BTN_START_HOTSPOT:
        StartHotspot();
        break;
    case ID_BTN_STOP_HOTSPOT:
        StopHotspot();
        break;
    case ID_BTN_WIFI_DIRECT_PAIR:
        OpenWifiDirectPairing();
        break;
    case ID_BTN_OPEN_HOTSPOT_SETTINGS:
        OpenSystemHotspotSettings();
        break;
    case ID_BTN_OPEN_CONNECTED_DEVICES:
        OpenWifiDirectPairing();
        break;
    case ID_BTN_OPEN_PAIRING_HELP:
        MessageBoxW(m_hwnd,
            L"1. Open Connected Devices.\r\n2. Pick the target device.\r\n3. Confirm pairing in Windows UI.\r\n4. Keep both devices on the same local link, then use the Viewer URL.",
            L"Pairing Help",
            MB_OK | MB_ICONINFORMATION);
        break;
    case ID_BTN_SELECT_ADAPTER_1:
    case ID_BTN_SELECT_ADAPTER_2:
    case ID_BTN_SELECT_ADAPTER_3:
    case ID_BTN_SELECT_ADAPTER_4:
        SelectNetworkCandidate(static_cast<std::size_t>(id - ID_BTN_SELECT_ADAPTER_1));
        break;
    case ID_BTN_DASH_SUGGESTION_FIX_1:
    case ID_BTN_DASH_SUGGESTION_FIX_2:
    case ID_BTN_DASH_SUGGESTION_FIX_3:
    case ID_BTN_DASH_SUGGESTION_FIX_4:
        ExecuteDashboardSuggestionFix(static_cast<std::size_t>((id - ID_BTN_DASH_SUGGESTION_FIX_1) / 10));
        break;
    case ID_BTN_DASH_SUGGESTION_INFO_1:
    case ID_BTN_DASH_SUGGESTION_INFO_2:
    case ID_BTN_DASH_SUGGESTION_INFO_3:
    case ID_BTN_DASH_SUGGESTION_INFO_4:
        ExecuteDashboardSuggestionInfo(static_cast<std::size_t>((id - ID_BTN_DASH_SUGGESTION_INFO_1) / 10));
        break;
    case ID_BTN_DASH_SUGGESTION_SETUP_1:
    case ID_BTN_DASH_SUGGESTION_SETUP_2:
    case ID_BTN_DASH_SUGGESTION_SETUP_3:
    case ID_BTN_DASH_SUGGESTION_SETUP_4:
        SetPage(UiPage::Setup);
        break;
    default:
        break;
    }
}


void MainWindow::EnsureHotspotDefaults() {
    if (!m_hotspotSsid.empty() && !m_hotspotPassword.empty()) {
        if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
        if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
        return;
    }

    const auto cfg = lan::network::NetworkManager::MakeSuggestedHotspotConfig();
    m_hotspotSsid = urlutil::Utf8ToWide(cfg.ssid);
    m_hotspotPassword = urlutil::Utf8ToWide(cfg.password);
    if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
    if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
}

void MainWindow::GenerateRoomToken() {
    m_room = urlutil::RandomAlnum(6);
    m_token = urlutil::RandomAlnum(16);
    m_viewerUrlCopied = false;
    m_shareCardExported = false;
    if (m_roomEdit) SetWindowTextW(m_roomEdit, m_room.c_str());
    if (m_tokenEdit) SetWindowTextW(m_tokenEdit, m_token.c_str());
    RefreshShareInfo();
}

void MainWindow::RefreshHotspotState() {
    lan::network::HotspotState state;
    std::string err;
    if (lan::network::NetworkManager::QueryHotspotState(state, err)) {
        m_hotspotRunning = state.running;
        m_hotspotStatus = state.running ? L"running" : L"stopped";
        if (!state.ssid.empty()) m_hotspotSsid = urlutil::Utf8ToWide(state.ssid);
        if (!state.password.empty()) m_hotspotPassword = urlutil::Utf8ToWide(state.password);
        if (!state.hostIp.empty()) {
            m_hostIp = urlutil::Utf8ToWide(state.hostIp);
            if (m_ipValue) SetWindowTextW(m_ipValue, m_hostIp.c_str());
        }
    } else {
        m_hotspotRunning = false;
        if (!err.empty()) AppendLog(L"Hotspot state probe: " + urlutil::Utf8ToWide(err));
    }
    if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
    if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::StartHotspot() {
    wchar_t buf[256]{};
    GetWindowTextW(m_hotspotSsidEdit, buf, _countof(buf));
    m_hotspotSsid = buf;
    GetWindowTextW(m_hotspotPwdEdit, buf, _countof(buf));
    m_hotspotPassword = buf;

    lan::network::HotspotConfig cfg;
    cfg.ssid = urlutil::WideToUtf8(m_hotspotSsid);
    cfg.password = urlutil::WideToUtf8(m_hotspotPassword);

    lan::network::HotspotState out;
    std::string err;
    if (lan::network::NetworkManager::StartHotspot(cfg, out, err)) {
        m_hotspotRunning = out.running;
        m_hotspotStatus = out.running ? L"running" : L"stopped";
        AppendLog(L"Hotspot started: " + m_hotspotSsid);
        AddTimelineEvent(L"Hotspot started");
        if (!out.hostIp.empty()) {
            m_hostIp = urlutil::Utf8ToWide(out.hostIp);
            if (m_ipValue) SetWindowTextW(m_ipValue, m_hostIp.c_str());
        }
    } else {
        m_hotspotRunning = false;
        m_hotspotStatus = L"start failed";
        AppendLog(L"Start hotspot failed: " + urlutil::Utf8ToWide(err));
        AddTimelineEvent(L"Hotspot start failed");
    }
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::StopHotspot() {
    std::string err;
    if (lan::network::NetworkManager::StopHotspot(err)) {
        m_hotspotRunning = false;
        m_hotspotStatus = L"stopped";
        AppendLog(L"Hotspot stopped");
        AddTimelineEvent(L"Hotspot stopped");
    } else {
        AppendLog(L"Stop hotspot failed: " + urlutil::Utf8ToWide(err));
    }
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::OpenWifiDirectPairing() {
    ShellExecuteW(m_hwnd, L"open", L"ms-settings:connecteddevices", nullptr, nullptr, SW_SHOWNORMAL);
    AppendLog(L"Opened Wi-Fi Direct pairing entry");
}

void MainWindow::OpenSystemHotspotSettings() {
    ShellExecuteW(m_hwnd, L"open", L"ms-settings:network-mobilehotspot", nullptr, nullptr, SW_SHOWNORMAL);
    AppendLog(L"Opened Windows Mobile Hotspot settings");
}

void MainWindow::OpenHostPage() {
    const auto url = BuildHostUrlLocal();
    ShellExecuteW(m_hwnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    AppendLog(L"Opened Host page");
    AddTimelineEvent(L"Host page opened in browser");
}

void MainWindow::OpenViewerPage() {
    const auto url = BuildViewerUrl();
    ShellExecuteW(m_hwnd, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    AppendLog(L"Opened Viewer page");
    AddTimelineEvent(L"Viewer page opened in browser");
}

void MainWindow::CopyHostUrl() {
    const auto url = BuildHostUrlLocal();
    if (urlutil::SetClipboardText(m_hwnd, url)) {
        AppendLog(L"Host URL copied");
    } else {
        AppendLog(L"Host URL copy failed");
    }
    RefreshDashboard();
}

void MainWindow::CopyViewerUrl() {
    const auto url = BuildViewerUrl();
    if (urlutil::SetClipboardText(m_hwnd, url)) {
        m_viewerUrlCopied = true;
        AppendLog(L"Viewer URL copied");
    } else {
        AppendLog(L"Viewer URL copy failed");
    }
    RefreshDashboard();
}

void MainWindow::ShowQr() {
    fs::path shareCardPath;
    if (WriteShareArtifacts(&shareCardPath, nullptr, nullptr, nullptr)) {
        m_shareCardExported = true;
        ShellExecuteW(m_hwnd, L"open", shareCardPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        AppendLog(L"Opened share card");
    }
    RefreshDashboard();
}

void MainWindow::ShowShareWizard() {
    fs::path shareWizardPath;
    if (WriteShareArtifacts(nullptr, &shareWizardPath, nullptr, nullptr)) {
        ShellExecuteW(m_hwnd, L"open", shareWizardPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        AppendLog(L"Opened share wizard");
    }
}

void MainWindow::ExportShareBundle() {
    fs::path bundleJsonPath;
    if (WriteShareArtifacts(nullptr, nullptr, &bundleJsonPath, nullptr)) {
        m_shareCardExported = true;
        OpenOutputFolder();
        AppendLog(L"Exported local share bundle");
        AddTimelineEvent(L"Diagnostics bundle generated");
    }
    RefreshDashboard();
}

void MainWindow::RunDesktopSelfCheck() {
    fs::path selfCheckPath;
    if (WriteShareArtifacts(nullptr, nullptr, nullptr, &selfCheckPath)) {
        ShellExecuteW(m_hwnd, L"open", selfCheckPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        AppendLog(L"Opened desktop self-check: " + selfCheckPath.wstring());
    }
}

void MainWindow::RefreshDiagnosticsBundle() {
    if (WriteShareArtifacts(nullptr, nullptr, nullptr, nullptr)) {
        AppendLog(L"Re-ran exported self-check and diagnostics bundle");
    } else {
        AppendLog(L"Re-run checks failed while exporting diagnostics bundle");
    }
}

void MainWindow::OpenDiagnosticsReport() {
    const fs::path diagnosticsFile = AppDir() / L"out" / L"share_bundle" / L"share_diagnostics.txt";
    if (!fs::exists(diagnosticsFile)) {
        WriteShareArtifacts(nullptr, nullptr, nullptr, nullptr);
    }
    if (fs::exists(diagnosticsFile)) {
        ShellExecuteW(m_hwnd, L"open", diagnosticsFile.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        AppendLog(L"Opened diagnostics report: " + diagnosticsFile.wstring());
    } else {
        AppendLog(L"Diagnostics report missing: " + diagnosticsFile.wstring());
    }
}

void MainWindow::OpenOutputFolder() {
    const fs::path outDir = AppDir() / L"out" / L"share_bundle";
    std::error_code ec;
    fs::create_directories(outDir, ec);
    ShellExecuteW(m_hwnd, L"open", outDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    AppendLog(L"Opened bundle folder: " + outDir.wstring());
}

bool MainWindow::WriteShareArtifacts(fs::path* shareCardPath,
                                     fs::path* shareWizardPath,
                                     fs::path* bundleJsonPath,
                                     fs::path* desktopSelfCheckPath) {
    const fs::path outDir = AppDir() / L"out" / L"share_bundle";
    const fs::path assetDir = outDir / L"www" / L"assets";
    std::error_code ec;
    fs::create_directories(assetDir, ec);
    if (ec) {
        AppendLog(L"Create share bundle dir failed: " + urlutil::Utf8ToWide(ec.message()));
        return false;
    }

    const fs::path qrAssetSrc = AppDir() / L"www" / L"assets" / L"share_card_qr.bundle.js";
    const fs::path qrAssetDst = assetDir / L"share_card_qr.bundle.js";
    if (fs::exists(qrAssetSrc)) {
        fs::copy_file(qrAssetSrc, qrAssetDst, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            AppendLog(L"Copy QR asset failed: " + urlutil::Utf8ToWide(ec.message()));
            ec.clear();
        }
    } else {
        AppendLog(L"Local QR asset missing: " + qrAssetSrc.wstring());
    }

    const auto hostUrl = BuildHostUrlLocal();
    const auto viewerUrl = BuildViewerUrl();
    const auto generatedAt = NowDateTime();
    const auto wifiDirectAlias = BuildWifiDirectSessionAlias();
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const auto selfCheckReport = BuildSelfCheckReport(m_hostPageState, m_hostIp, viewerUrl, m_lastViewers,
                                                      runtime.serverProcessRunning,
                                                      certInfo.certExists, certInfo.keyExists,
                                                      runtime.portReady, runtime.portDetail,
                                                      runtime.localHealthReady, runtime.localHealthDetail,
                                                      runtime.hostIpReachable, runtime.hostIpReachableDetail,
                                                      runtime.lanBindReady, runtime.lanBindDetail,
                                                      runtime.activeIpv4Candidates, runtime.selectedIpRecommended, runtime.adapterHint,
                                                      runtime.embeddedHostReady, runtime.embeddedHostStatus,
                                                      m_wifiAdapterPresent, m_hotspotSupported,
                                                      m_wifiDirectApiAvailable, m_hotspotRunning, true);
    const std::string bundleJson = BuildShareBundleJson(
        m_networkMode.empty() ? L"unknown" : m_networkMode,
        m_hostIp,
        m_port,
        m_room,
        m_token,
        m_hostPageState,
        m_lastRooms,
        m_lastViewers,
        m_hotspotSsid,
        m_hotspotPassword,
        m_hotspotRunning,
        m_wifiAdapterPresent,
        m_hotspotSupported,
        m_wifiDirectApiAvailable,
        wifiDirectAlias,
        hostUrl,
        viewerUrl,
        generatedAt,
        runtime.serverProcessRunning,
        runtime.portReady,
        runtime.portDetail,
        runtime.localHealthReady,
        runtime.localHealthDetail,
        runtime.hostIpReachable,
        runtime.hostIpReachableDetail,
        runtime.lanBindReady,
        runtime.lanBindDetail,
        runtime.activeIpv4Candidates,
        runtime.selectedIpRecommended,
        runtime.adapterHint,
        runtime.embeddedHostReady,
        runtime.embeddedHostStatus,
        certInfo.certDir.wstring(),
        certInfo.certFile.wstring(),
        certInfo.keyFile.wstring(),
        certInfo.certExists,
        certInfo.keyExists);

    const fs::path shareCard = outDir / L"share_card.html";
    const fs::path shareWizard = outDir / L"share_wizard.html";
    const fs::path bundleJsonFile = outDir / L"share_bundle.json";
    const fs::path statusJsFile = outDir / L"share_status.js";
    const fs::path diagnosticsFile = outDir / L"share_diagnostics.txt";
    const fs::path desktopSelfCheckHtmlFile = outDir / L"desktop_self_check.html";
    const fs::path desktopSelfCheckTxtFile = outDir / L"desktop_self_check.txt";
    const fs::path viewerUrlFile = outDir / L"viewer_url.txt";
    const fs::path hotspotCredFile = outDir / L"hotspot_credentials.txt";
    const fs::path readmeFile = outDir / L"share_readme.txt";

    auto writeUtf8 = [](const fs::path& path, const std::string& content) -> bool {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        return f.good();
    };

    if (!writeUtf8(shareCard, BuildShareCardHtml(m_networkMode, m_hostIp, m_port, m_room, m_token, m_hostPageState,
                                                 m_lastRooms, m_lastViewers, m_hotspotSsid, m_hotspotPassword,
                                                 m_hotspotRunning, m_wifiDirectApiAvailable, hostUrl, viewerUrl,
                                                 bundleJson))) {
        AppendLog(L"Write share_card.html failed");
        return false;
    }
    if (!writeUtf8(shareWizard, BuildShareWizardHtml(bundleJson))) {
        AppendLog(L"Write share_wizard.html failed");
        return false;
    }
    if (!writeUtf8(bundleJsonFile, bundleJson)) {
        AppendLog(L"Write share_bundle.json failed");
        return false;
    }
    writeUtf8(statusJsFile, BuildShareStatusJs(bundleJson));
    writeUtf8(diagnosticsFile, BuildShareDiagnosticsText(generatedAt, m_networkMode, m_hostIp, m_port, m_room, m_hostPageState,
                                                         m_lastRooms, m_lastViewers, m_hotspotStatus, m_hotspotSsid,
                                                         m_wifiAdapterPresent, m_hotspotSupported, m_wifiDirectApiAvailable,
                                                         hostUrl, viewerUrl,
                                                         runtime.serverProcessRunning,
                                                         runtime.portReady,
                                                         runtime.portDetail,
                                                         runtime.localHealthReady,
                                                         runtime.localHealthDetail,
                                                         runtime.hostIpReachable,
                                                         runtime.hostIpReachableDetail,
                                                         runtime.lanBindReady,
                                                         runtime.lanBindDetail,
                                                         runtime.activeIpv4Candidates,
                                                         runtime.selectedIpRecommended,
                                                         runtime.adapterHint,
                                                         runtime.embeddedHostReady,
                                                         runtime.embeddedHostStatus,
                                                         certInfo.certDir.wstring(), certInfo.certFile.wstring(), certInfo.keyFile.wstring(),
                                                         certInfo.certExists, certInfo.keyExists));
    writeUtf8(desktopSelfCheckHtmlFile, BuildDesktopSelfCheckHtml(bundleJson));
    writeUtf8(desktopSelfCheckTxtFile, BuildDesktopSelfCheckText(generatedAt, hostUrl, viewerUrl, selfCheckReport));
    writeUtf8(viewerUrlFile, urlutil::WideToUtf8(viewerUrl) + "\r\n");
    writeUtf8(hotspotCredFile,
              std::string("SSID: ") + urlutil::WideToUtf8(m_hotspotSsid) + "\r\nPassword: " +
              urlutil::WideToUtf8(m_hotspotPassword) + "\r\nStatus: " + urlutil::WideToUtf8(m_hotspotStatus) + "\r\n");
    writeUtf8(readmeFile, BuildShareReadmeText(hostUrl, viewerUrl, m_hotspotSsid, m_hotspotPassword, m_hotspotRunning, wifiDirectAlias));

    if (shareCardPath) *shareCardPath = shareCard;
    if (shareWizardPath) *shareWizardPath = shareWizard;
    if (bundleJsonPath) *bundleJsonPath = bundleJsonFile;
    if (desktopSelfCheckPath) *desktopSelfCheckPath = desktopSelfCheckHtmlFile;
    return true;
}

std::wstring MainWindow::BuildWifiDirectSessionAlias() const {
    return L"LanShare-" + (m_room.empty() ? std::wstring(L"session") : m_room);
}

void MainWindow::NavigateHostInWebView() {
    const auto url = BuildHostUrlLocal();
    m_webviewMode = WebViewSurfaceMode::HostPreview;
    m_webview.Navigate(url);
    AppendLog(L"Embedded host page navigate: " + url);
}

void MainWindow::NavigateHtmlAdminInWebView() {
    const fs::path uiDir = AdminUiDir();
    const fs::path indexFile = uiDir / L"index.html";
    if (!fs::exists(indexFile)) {
        AppendLog(L"HTML admin shell missing: " + indexFile.wstring());
        m_webviewMode = WebViewSurfaceMode::Hidden;
        return;
    }

    if (m_webviewMode == WebViewSurfaceMode::HtmlAdminPreview) {
        return;
    }

    m_adminShellReady = false;
    m_webviewMode = WebViewSurfaceMode::HtmlAdminPreview;
    m_webview.Navigate(BuildFileUrl(indexFile));
    AppendLog(L"HTML admin shell navigate: " + indexFile.wstring());
}

void MainWindow::RefreshHtmlAdminPreview() {
    if (!IsHtmlAdminActive()) return;
    if (!m_adminShellReady) return;
    if (!m_adminBackend) return;
    m_webview.PostJson(m_adminBackend->BuildSnapshotEventJson(BuildAdminSnapshot()));
}

void MainWindow::HandleAdminShellMessage(std::wstring_view payload) {
    if (!m_adminBackend) return;

    const auto result = m_adminBackend->HandleMessage(payload);
    if (!result.logLine.empty()) {
        AppendLog(result.logLine);
    }
    if (result.requestSnapshot) {
        m_adminShellReady = true;
    }
    if (result.requestSnapshot || result.stateChanged) {
        RefreshHtmlAdminPreview();
    }
}

AdminBackend::Snapshot MainWindow::BuildAdminSnapshot() const {
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto candidates = CollectActiveIpv4Candidates();
    const auto viewerUrl = BuildViewerUrl();
    const auto report = BuildSelfCheckReport(m_hostPageState, m_hostIp, viewerUrl, m_lastViewers,
                                             runtime.serverProcessRunning,
                                             certInfo.certExists, certInfo.keyExists,
                                             runtime.portReady, runtime.portDetail,
                                             runtime.localHealthReady, runtime.localHealthDetail,
                                             runtime.hostIpReachable, runtime.hostIpReachableDetail,
                                             runtime.lanBindReady, runtime.lanBindDetail,
                                             runtime.activeIpv4Candidates, runtime.selectedIpRecommended, runtime.adapterHint,
                                             runtime.embeddedHostReady, runtime.embeddedHostStatus,
                                             m_wifiAdapterPresent, m_hotspotSupported, m_wifiDirectApiAvailable, m_hotspotRunning, true);

    AdminBackend::Snapshot snapshot;
    snapshot.appName = L"LanScreenShareHostApp";
    snapshot.nativePage = AdminTabNameForPage(m_currentPage);
    snapshot.dashboardState = L"not-ready";
    snapshot.dashboardLabel = L"Not Ready";
    if (IsHostStateSharing(m_hostPageState)) {
        snapshot.dashboardState = L"sharing";
        snapshot.dashboardLabel = L"Sharing";
    } else if (report.p0 == 0 && runtime.serverProcessRunning && IsHostStateReadyOrLoading(m_hostPageState)) {
        snapshot.dashboardState = L"ready";
        snapshot.dashboardLabel = L"Ready";
    } else if (runtime.serverProcessRunning && (!runtime.localHealthReady || !runtime.hostIpReachable)) {
        snapshot.dashboardState = L"error";
        snapshot.dashboardLabel = L"Error";
    }

    snapshot.dashboardError = m_lastErrorSummary;
    if (!report.failures.empty()) {
        snapshot.dashboardError = urlutil::Utf8ToWide(report.failures.front().title);
    }
    if (snapshot.dashboardError.empty()) snapshot.dashboardError = L"none";

    snapshot.canStartSharing = !IsHostStateSharing(m_hostPageState);
    snapshot.sharingActive = IsHostStateSharing(m_hostPageState);
    snapshot.serverRunning = runtime.serverProcessRunning;
    snapshot.healthReady = runtime.localHealthReady;
    snapshot.hostReachable = runtime.hostIpReachable;
    snapshot.certReady = certInfo.certExists && certInfo.keyExists;
    snapshot.wifiAdapterPresent = m_wifiAdapterPresent;
    snapshot.hotspotSupported = m_hotspotSupported;
    snapshot.hotspotRunning = m_hotspotRunning;
    snapshot.wifiDirectAvailable = m_wifiDirectApiAvailable;
    snapshot.activeIpv4Candidates = runtime.activeIpv4Candidates;
    snapshot.port = m_port;
    snapshot.rooms = m_lastRooms;
    snapshot.viewers = m_lastViewers;
    snapshot.hostIp = m_hostIp.empty() ? L"(not found)" : m_hostIp;
    snapshot.bind = m_bindAddress;
    snapshot.room = m_room;
    snapshot.token = m_token;
    snapshot.hostUrl = BuildHostUrlLocal();
    snapshot.viewerUrl = viewerUrl;
    snapshot.networkMode = m_networkMode.empty() ? L"unknown" : m_networkMode;
    snapshot.hostState = m_hostPageState;
    snapshot.hotspotStatus = m_hotspotStatus;
    snapshot.hotspotSsid = m_hotspotSsid;
    snapshot.hotspotPassword = m_hotspotPassword;
    snapshot.webviewStatus = m_webview.StatusText();
    snapshot.recentHeartbeat = runtime.localHealthReady ? L"/health ok" : runtime.localHealthDetail;
    snapshot.localReachability = runtime.hostIpReachable ? L"ok" : runtime.hostIpReachableDetail;
    snapshot.outputDir = (AppDir() / L"out").wstring();
    snapshot.bundleDir = (AppDir() / L"out" / L"share_bundle").wstring();
    snapshot.serverExePath = (AppDir() / L"lan_screenshare_server.exe").wstring();
    snapshot.certDir = certInfo.certDir.wstring();
    snapshot.timelineText = m_timelineText.empty() ? L"No timeline events yet." : m_timelineText;
    snapshot.logTail = m_logs.size() > 8000 ? m_logs.substr(m_logs.size() - 8000) : m_logs;
    snapshot.viewerUrlCopied = m_viewerUrlCopied;
    snapshot.shareBundleExported = m_shareCardExported;
    snapshot.lastError = m_lastErrorSummary;
    snapshot.defaultPort = m_defaultPort;
    snapshot.defaultBind = m_defaultBindAddress;
    snapshot.roomRule = m_roomRule;
    snapshot.tokenRule = m_tokenRule;
    snapshot.logLevel = m_logLevel;
    snapshot.defaultViewerOpenMode = m_defaultViewerOpenMode;
    snapshot.autoCopyViewerLink = m_autoCopyViewerLink;
    snapshot.autoGenerateQr = m_autoGenerateQr;
    snapshot.autoExportBundle = m_autoExportBundle;
    snapshot.saveStdStreams = m_saveStdStreams;
    snapshot.certBypassPolicy = m_certBypassPolicy;
    snapshot.webViewBehavior = m_webViewBehavior;
    snapshot.startupHook = m_startupHook;
    for (std::size_t i = 0; i < candidates.size() && i < 6; ++i) {
        const auto& candidate = candidates[i];
        AdminBackend::AdapterCandidate item;
        item.name = candidate.name;
        item.ip = candidate.ip;
        item.type = (WideContainsCaseInsensitive(candidate.name, L"wi-fi") || WideContainsCaseInsensitive(candidate.name, L"wireless"))
            ? L"Wi-Fi"
            : L"Ethernet / Other";
        item.recommended = i == 0;
        item.selected = candidate.ip == m_hostIp;
        snapshot.networkCandidates.push_back(std::move(item));
    }
    return snapshot;
}

void MainWindow::ApplySessionConfigFromAdmin(std::wstring room, std::wstring token, std::wstring bind, int port) {
    m_room = std::move(room);
    m_token = std::move(token);
    m_bindAddress = bind.empty() ? std::wstring(L"0.0.0.0") : std::move(bind);
    if (port > 0 && port <= 65535) {
        m_port = port;
    } else if (m_port <= 0) {
        m_port = 9443;
    }

    if (m_roomEdit) SetWindowTextW(m_roomEdit, m_room.c_str());
    if (m_tokenEdit) SetWindowTextW(m_tokenEdit, m_token.c_str());
    if (m_bindEdit) SetWindowTextW(m_bindEdit, m_bindAddress.c_str());
    if (m_portEdit) {
        const std::wstring portText = std::to_wstring(m_port);
        SetWindowTextW(m_portEdit, portText.c_str());
    }

    m_viewerUrlCopied = false;
    m_shareCardExported = false;
    AppendLog(L"Admin shell applied session config");
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::ApplyHotspotConfigFromAdmin(std::wstring ssid, std::wstring password) {
    m_hotspotSsid = std::move(ssid);
    m_hotspotPassword = std::move(password);
    if (m_hotspotSsid.empty() || m_hotspotPassword.empty()) {
        EnsureHotspotDefaults();
    }
    if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
    if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
    AppendLog(L"Admin shell applied hotspot config");
    RefreshNetworkPage();
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::AppendLog(std::wstring_view line) {
    const std::wstring ts = NowTs();
    std::wstring entry = L"[" + ts + L"] " + std::wstring(line) + L"\r\n";
    m_logs += entry;
    if (m_logs.size() > 32000) {
        m_logs.erase(0, m_logs.size() - 28000);
    }
    if (m_logBox) SetWindowTextW(m_logBox, m_logs.c_str());
    m_logEntries.push_back(LogEntry{ts, DetectLogLevel(line), DetectLogSource(line), std::wstring(line)});
    if (m_logEntries.size() > 400) {
        m_logEntries.erase(m_logEntries.begin(), m_logEntries.begin() + 120);
    }
    if (WideContainsCaseInsensitive(line, L"failed") ||
        WideContainsCaseInsensitive(line, L"error") ||
        WideContainsCaseInsensitive(line, L"blocked") ||
        WideContainsCaseInsensitive(line, L"missing")) {
        m_lastErrorSummary = std::wstring(line);
    }
    RefreshDashboard();
    RefreshDiagnosticsPage();
}

void MainWindow::RefreshDashboard() {
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const auto viewerUrl = BuildViewerUrl();
    const auto report = BuildSelfCheckReport(m_hostPageState, m_hostIp, viewerUrl, m_lastViewers,
                                             runtime.serverProcessRunning,
                                             certInfo.certExists, certInfo.keyExists,
                                             runtime.portReady, runtime.portDetail,
                                             runtime.localHealthReady, runtime.localHealthDetail,
                                             runtime.hostIpReachable, runtime.hostIpReachableDetail,
                                             runtime.lanBindReady, runtime.lanBindDetail,
                                             runtime.activeIpv4Candidates, runtime.selectedIpRecommended, runtime.adapterHint,
                                             runtime.embeddedHostReady, runtime.embeddedHostStatus,
                                             m_wifiAdapterPresent, m_hotspotSupported, m_wifiDirectApiAvailable, m_hotspotRunning, true);

    std::wstring overall = L"Not Ready";
    if (IsHostStateSharing(m_hostPageState)) {
        overall = L"Sharing";
    } else if (report.p0 == 0 && runtime.serverProcessRunning && IsHostStateReadyOrLoading(m_hostPageState)) {
        overall = L"Ready";
    } else if (runtime.serverProcessRunning && (!runtime.localHealthReady || !runtime.hostIpReachable)) {
        overall = L"Error";
    }

    std::wstring errorSummary = m_lastErrorSummary;
    if (!report.failures.empty()) {
        errorSummary = urlutil::Utf8ToWide(report.failures.front().title);
    }
    if (errorSummary.empty()) errorSummary = L"none";

    std::wstringstream statusCard;
    statusCard << L"Current State: " << overall << L"\r\n";
    statusCard << L"Host IP: " << (m_hostIp.empty() ? L"(not found)" : m_hostIp) << L"\r\n";
    statusCard << L"Port: " << m_port << L"\r\n";
    statusCard << L"Room: " << m_room << L"\r\n";
    statusCard << L"Viewer Count: " << m_lastViewers << L"\r\n";
    statusCard << L"Latest Error: " << errorSummary;
    SetTextIfPresent(m_dashboardStatusCard, statusCard.str());

    std::wstringstream networkCard;
    networkCard << L"Network\r\n";
    networkCard << L"Primary IPv4: " << (m_hostIp.empty() ? L"(not found)" : m_hostIp) << L"\r\n";
    networkCard << L"Adapter Count: " << runtime.activeIpv4Candidates << L"\r\n";
    networkCard << L"Wi-Fi / Hotspot: "
                << (m_wifiAdapterPresent ? L"Wi-Fi yes" : L"Wi-Fi no")
                << L", hotspot " << (m_hotspotSupported ? L"yes" : L"fallback")
                << L", Wi-Fi Direct " << (m_wifiDirectApiAvailable ? L"yes" : L"no");
    SetTextIfPresent(m_dashboardNetworkCard, networkCard.str());

    std::wstringstream serviceCard;
    serviceCard << L"Service\r\n";
    serviceCard << L"Server Exe: " << (AppDir() / L"lan_screenshare_server.exe").wstring() << L"\r\n";
    serviceCard << L"Bind + Port: " << m_bindAddress << L":" << m_port << L"\r\n";
    serviceCard << L"Cert State: " << ((certInfo.certExists && certInfo.keyExists) ? L"ready" : L"missing");
    SetTextIfPresent(m_dashboardServiceCard, serviceCard.str());

    std::wstringstream shareCard;
    shareCard << L"Sharing\r\n";
    shareCard << L"Viewer URL: " << viewerUrl << L"\r\n";
    shareCard << L"Copied: " << (m_viewerUrlCopied ? L"yes" : L"no") << L"\r\n";
    shareCard << L"Share Card Exported: " << (m_shareCardExported ? L"yes" : L"no");
    SetTextIfPresent(m_dashboardShareCard, shareCard.str());

    std::wstringstream healthCard;
    healthCard << L"Health\r\n";
    healthCard << L"/health: " << (runtime.localHealthReady ? L"ok" : L"attention") << L"\r\n";
    healthCard << L"/api/status: " << (runtime.serverProcessRunning ? L"polling" : L"server stopped") << L"\r\n";
    healthCard << L"WebView: " << m_webview.StatusText() << L"\r\n";
    healthCard << L"Local Reachability: " << (runtime.hostIpReachable ? L"ok" : L"attention");
    SetTextIfPresent(m_dashboardHealthCard, healthCard.str());

    EnableWindow(m_dashboardPrimaryBtn, IsHostStateSharing(m_hostPageState) ? FALSE : TRUE);

    for (auto& kind : m_dashboardSuggestionKinds) kind = DashboardSuggestionKind::None;
    for (auto& detail : m_dashboardSuggestionDetails) detail.clear();

    std::size_t slot = 0;
    auto addSuggestion = [&](DashboardSuggestionKind kind, std::wstring title, std::wstring detail) {
        if (slot >= m_dashboardSuggestionKinds.size()) return;
        m_dashboardSuggestionKinds[slot] = kind;
        m_dashboardSuggestionDetails[slot] = std::move(detail);
        SetTextIfPresent(m_dashboardSuggestionText[slot], std::move(title));
        EnableWindow(m_dashboardSuggestionFixBtn[slot], TRUE);
        EnableWindow(m_dashboardSuggestionInfoBtn[slot], TRUE);
        EnableWindow(m_dashboardSuggestionSetupBtn[slot], TRUE);
        ++slot;
    };

    if (!runtime.embeddedHostReady) {
        addSuggestion(DashboardSuggestionKind::OpenHostExternally,
                      L"WebView2 runtime is unavailable",
                      L"Embedded host view is unavailable. Open the host page in an external browser, or install/repair WebView2 Runtime.");
    }
    if (!m_hotspotRunning) {
        addSuggestion(m_hotspotSupported ? DashboardSuggestionKind::StartHotspot : DashboardSuggestionKind::OpenHotspotSettings,
                      L"Hotspot is not running",
                      m_hotspotSupported ? L"You can try starting hotspot directly. If that fails, open Windows Mobile Hotspot settings." : L"This machine does not expose controllable hotspot support. Open system hotspot settings.");
    }
    if (certInfo.certExists && certInfo.keyExists) {
        addSuggestion(DashboardSuggestionKind::NoteSelfSignedCert,
                      L"Certificate is self-signed",
                      L"On first LAN access, the browser may prompt for certificate trust. That is expected for the current local HTTPS MVP.");
    }
    if (runtime.portReady) {
        addSuggestion(DashboardSuggestionKind::PortReady,
                      std::wstring(L"Port ") + std::to_wstring(m_port) + L" is available",
                      runtime.portDetail.empty() ? L"The configured port is available for the local server." : runtime.portDetail);
    }
    if (slot == 0 || slot < m_dashboardSuggestionKinds.size()) {
        if (!runtime.serverProcessRunning) {
            addSuggestion(DashboardSuggestionKind::StartServer,
                          L"Local server is not started",
                          L"Use Start Sharing to launch the local HTTPS/WSS server and load the host page.");
        }
    }
    if (slot < m_dashboardSuggestionKinds.size() && m_hostIp.empty()) {
        addSuggestion(DashboardSuggestionKind::RefreshIp,
                      L"Host IP is still missing",
                      L"Refresh the host IPv4, or connect to LAN / start hotspot before sharing.");
    }
    while (slot < m_dashboardSuggestionKinds.size()) {
        SetTextIfPresent(m_dashboardSuggestionText[slot], L"No higher-priority suggestion right now.");
        m_dashboardSuggestionKinds[slot] = DashboardSuggestionKind::OpenDiagnostics;
        m_dashboardSuggestionDetails[slot] = L"Open diagnostics to inspect the full runtime snapshot.";
        EnableWindow(m_dashboardSuggestionFixBtn[slot], FALSE);
        EnableWindow(m_dashboardSuggestionInfoBtn[slot], TRUE);
        EnableWindow(m_dashboardSuggestionSetupBtn[slot], TRUE);
        ++slot;
    }
}

void MainWindow::RefreshSessionSetup() {
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const std::wstring hostUrl = BuildHostUrlLocal();
    const std::wstring viewerUrl = BuildViewerUrl();

    if (m_sanIpValue) {
        std::wstring san = m_hostIp.empty() ? L"127.0.0.1" : (m_hostIp + L",127.0.0.1");
        SetWindowTextW(m_sanIpValue, san.c_str());
    }

    if (m_sessionSummaryBox) {
        std::wstringstream ss;
        ss << L"Host URL\r\n" << hostUrl << L"\r\n\r\n";
        ss << L"Viewer URL\r\n" << viewerUrl << L"\r\n\r\n";
        ss << L"Output Dir\r\n" << (AppDir() / L"out").wstring() << L"\r\n\r\n";
        ss << L"Share Bundle\r\n" << (AppDir() / L"out" / L"share_bundle").wstring();
        SetWindowTextW(m_sessionSummaryBox, ss.str().c_str());
    }

    if (m_runtimeInfoCard) {
        std::wstringstream ss;
        ss << L"Runtime Info\r\n";
        ss << L"Uptime: " << (runtime.serverProcessRunning ? L"running" : L"stopped") << L"\r\n";
        ss << L"Rooms: " << m_lastRooms << L"\r\n";
        ss << L"Viewers: " << m_lastViewers << L"\r\n";
        ss << L"Recent Heartbeat: " << (runtime.localHealthReady ? L"/health ok" : runtime.localHealthDetail) << L"\r\n";
        ss << L"Host Ready State: " << m_hostPageState << L"\r\n";
        ss << L"Local Reachability: " << (runtime.hostIpReachable ? L"ok" : runtime.hostIpReachableDetail);
        SetWindowTextW(m_runtimeInfoCard, ss.str().c_str());
    }

    if (m_hostPreviewPlaceholder) {
        std::wstringstream ss;
        ss << L"Host Preview Unavailable\r\n\r\n";
        if (runtime.embeddedHostStatus == L"sdk-unavailable" || runtime.embeddedHostStatus == L"runtime-unavailable" || runtime.embeddedHostStatus == L"controller-unavailable") {
            ss << L"Reason: WebView2 Runtime / SDK is unavailable.\r\n\r\n";
        } else {
            ss << L"Reason: Host preview is not ready yet.\r\n\r\n";
        }
        ss << L"Action: Open Host in the system browser.\r\n";
        ss << L"Host URL: " << hostUrl << L"\r\n";
        ss << L"Cert State: " << ((certInfo.certExists && certInfo.keyExists) ? L"ready" : L"missing");
        SetWindowTextW(m_hostPreviewPlaceholder, ss.str().c_str());
        const bool placeholderVisible = !m_webview.IsReady() && !IsHtmlAdminActive();
        ShowWindow(m_hostPreviewPlaceholder, (m_currentPage == UiPage::Setup && placeholderVisible) ? SW_SHOW : SW_HIDE);
        if (m_btnOpenHost) {
            ShowWindow(m_btnOpenHost, (m_currentPage == UiPage::Setup && !IsHtmlAdminActive()) ? SW_SHOW : SW_HIDE);
        }
    }
}

void MainWindow::RefreshNetworkPage() {
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const auto candidates = CollectActiveIpv4Candidates();

    if (m_networkSummaryCard) {
        std::wstringstream ss;
        ss << L"Current Recommended IPv4: " << (candidates.empty() ? L"(none)" : candidates.front().ip) << L"\r\n";
        ss << L"Adapter: " << (candidates.empty() ? L"(none)" : candidates.front().name) << L"\r\n";
        ss << L"Network Type: " << (m_networkMode.empty() ? L"unknown" : m_networkMode) << L"\r\n";
        ss << L"Multiple Candidate IPs: " << (candidates.size() > 1 ? L"yes" : L"no") << L"\r\n";
        ss << L"Reachability: " << (runtime.hostIpReachable ? L"reachable" : runtime.hostIpReachableDetail);
        SetWindowTextW(m_networkSummaryCard, ss.str().c_str());
    }

    for (std::size_t i = 0; i < m_networkAdapterCards.size(); ++i) {
        std::wstringstream ss;
        bool hasCandidate = i < candidates.size();
        if (hasCandidate) {
            const auto& c = candidates[i];
            const bool isWifi = WideContainsCaseInsensitive(c.name, L"wi-fi") || WideContainsCaseInsensitive(c.name, L"wireless");
            const bool recommended = i == 0;
            const bool selected = c.ip == m_hostIp;
            m_networkCandidateIps[i] = c.ip;
            ss << L"Adapter: " << c.name << L"\r\n";
            ss << L"IPv4: " << c.ip << L"\r\n";
            ss << L"Type: " << (isWifi ? L"Wi-Fi" : L"Ethernet / Other") << L" | Online: yes\r\n";
            ss << L"Recommended: " << (recommended ? L"yes" : L"no") << L" | Selected: " << (selected ? L"yes" : L"no");
        } else {
            m_networkCandidateIps[i].clear();
            ss << L"No adapter candidate in this slot.";
        }
        if (m_networkAdapterCards[i]) SetWindowTextW(m_networkAdapterCards[i], ss.str().c_str());
        if (m_networkAdapterSelectBtns[i]) EnableWindow(m_networkAdapterSelectBtns[i], hasCandidate ? TRUE : FALSE);
    }

    if (m_hotspotStatusCard) {
        std::wstring hotspotState = L"Not Started";
        if (m_hotspotRunning) hotspotState = L"Started";
        else if (!m_hotspotSupported) hotspotState = L"System Managed / Not Supported";
        std::wstringstream ss;
        ss << L"Hotspot Status: " << hotspotState << L"\r\n";
        ss << L"SSID: " << (m_hotspotSsid.empty() ? L"(not set)" : m_hotspotSsid) << L"\r\n";
        ss << L"Password: " << (m_hotspotPassword.empty() ? L"(not set)" : m_hotspotPassword) << L"\r\n";
        ss << L"Mode: " << (m_hotspotSupported ? L"best-effort control" : L"system takeover / unsupported");
        SetWindowTextW(m_hotspotStatusCard, ss.str().c_str());
    }

    if (m_wifiDirectCard) {
        std::wstringstream ss;
        ss << L"Wi-Fi Direct Supported: " << (m_wifiDirectApiAvailable ? L"yes" : L"no") << L"\r\n";
        ss << L"Recommended Action: "
           << (m_wifiDirectApiAvailable ? L"Open Connected Devices and complete pairing in Windows UI." : L"Use system settings or stay on the same LAN / hotspot.") << L"\r\n";
        ss << L"Current Session Alias: " << BuildWifiDirectSessionAlias();
        SetWindowTextW(m_wifiDirectCard, ss.str().c_str());
    }
}

void MainWindow::RefreshSharingPage() {
    const std::wstring hostUrl = BuildHostUrlLocal();
    const std::wstring viewerUrl = BuildViewerUrl();

    if (m_accessEntryCard) {
        std::wstringstream ss;
        ss << L"Access Entry\r\n\r\n";
        ss << L"Host URL\r\n" << hostUrl << L"\r\n\r\n";
        ss << L"Viewer URL\r\n" << viewerUrl << L"\r\n\r\n";
        ss << L"Use Viewer URL for guests. Use Host URL only for the operator host page.";
        SetWindowTextW(m_accessEntryCard, ss.str().c_str());
    }

    if (m_qrCard) {
        std::wstringstream ss;
        ss << L"QR & Share Materials\r\n\r\n";
        ss << L"Viewer QR: available via share card / share wizard\r\n";
        ss << L"Host QR: advanced operator path only\r\n\r\n";
        ss << L"Recommended actions\r\n";
        ss << L"- Open Share Card for a large QR preview\r\n";
        ss << L"- Open Share Wizard for guided access handoff\r\n";
        ss << L"- Open Bundle Folder for offline materials\r\n";
        ss << L"- Export Offline Package to refresh all generated assets";
        SetWindowTextW(m_qrCard, ss.str().c_str());
    }

    if (m_accessGuideCard) {
        std::wstringstream ss;
        ss << L"Access Guide\r\n\r\n";
        ss << L"Same LAN\r\n";
        ss << L"- Keep both devices on the same router or switch.\r\n";
        ss << L"- Send the Viewer URL or show the Viewer QR.\r\n\r\n";
        ss << L"Hotspot Mode\r\n";
        ss << L"- Start hotspot in Network page.\r\n";
        ss << L"- Join that hotspot from the guest device.\r\n";
        ss << L"- Open the Viewer URL exactly as shown.\r\n\r\n";
        ss << L"Certificate Reminder\r\n";
        ss << L"- First access may show a self-signed certificate prompt.\r\n";
        ss << L"- Accept it for this local session.\r\n\r\n";
        ss << L"Common Failure Reasons\r\n";
        ss << L"- Devices are not on the same local network.\r\n";
        ss << L"- Wrong IP was selected as the main host address.\r\n";
        ss << L"- Local service is not running yet.\r\n";
        ss << L"- Browser blocked the self-signed certificate.";
        SetWindowTextW(m_accessGuideCard, ss.str().c_str());
    }
}

void MainWindow::RefreshMonitorPage() {
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const std::array<std::wstring, 5> metricTexts = {
        std::wstring(L"Rooms\n") + std::to_wstring(m_lastRooms),
        std::wstring(L"Viewers\n") + std::to_wstring(m_lastViewers),
        std::wstring(L"Host State\n") + m_hostPageState,
        std::wstring(L"/health\n") + (runtime.localHealthReady ? L"OK" : L"ATTN"),
        std::wstring(L"Latency\n") + (runtime.localHealthReady ? L"<1s" : L"n/a")
    };
    for (std::size_t i = 0; i < m_monitorMetricCards.size(); ++i) {
        if (m_monitorMetricCards[i]) SetWindowTextW(m_monitorMetricCards[i], metricTexts[i].c_str());
    }

    if (m_monitorTimelineBox) {
        std::wstring value = m_timelineText.empty() ? L"No timeline events yet." : m_timelineText;
        SetWindowTextW(m_monitorTimelineBox, value.c_str());
    }

    if (m_monitorDetailBox) {
        std::wstringstream ss;
        ss << L"Health Checks\r\n";
        ss << L"- Local /health: " << (runtime.localHealthReady ? L"green / normal" : L"red / abnormal") << L"\r\n";
        ss << L"- Host reachability: " << (runtime.hostIpReachable ? L"green / normal" : L"yellow / attention") << L"\r\n";
        ss << L"- WebView: " << (runtime.embeddedHostReady ? L"green / normal" : L"gray / inactive") << L"\r\n\r\n";
        ss << L"Connection Events\r\n";
        ss << L"- Rooms: " << m_lastRooms << L"\r\n";
        ss << L"- Viewers: " << m_lastViewers << L"\r\n\r\n";
        ss << L"Realtime Logs\r\n";
        ss << L"- Info / Warning / Error / WebView / Server stdout-stderr are currently merged below.\r\n\r\n";
        ss << m_logs;
        SetWindowTextW(m_monitorDetailBox, ss.str().c_str());
    }
}

void MainWindow::RefreshFilteredLogs() {
    if (!m_diagLogViewer) return;
    wchar_t searchBuf[256]{};
    if (m_diagLogSearch) GetWindowTextW(m_diagLogSearch, searchBuf, _countof(searchBuf));
    const std::wstring search = searchBuf;
    const int levelSel = m_diagLevelFilter ? static_cast<int>(SendMessageW(m_diagLevelFilter, CB_GETCURSEL, 0, 0)) : 0;
    const int sourceSel = m_diagSourceFilter ? static_cast<int>(SendMessageW(m_diagSourceFilter, CB_GETCURSEL, 0, 0)) : 0;
    const std::wstring levelFilter = levelSel == 1 ? L"Info" : levelSel == 2 ? L"Warning" : levelSel == 3 ? L"Error" : L"";
    const std::wstring sourceFilter = sourceSel == 1 ? L"app" : sourceSel == 2 ? L"network" : sourceSel == 3 ? L"server" : sourceSel == 4 ? L"webview" : L"";

    std::wstringstream ss;
    for (const auto& entry : m_logEntries) {
        if (!levelFilter.empty() && entry.level != levelFilter) continue;
        if (!sourceFilter.empty() && entry.source != sourceFilter) continue;
        if (!search.empty() && !WideContainsCaseInsensitive(entry.message, search)) continue;
        ss << L"[" << entry.timestamp << L"][" << entry.level << L"][" << entry.source << L"] " << entry.message << L"\r\n";
    }
    SetWindowTextW(m_diagLogViewer, ss.str().c_str());
}

void MainWindow::RefreshDiagnosticsPage() {
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    if (m_diagChecklistCard) {
        std::wstringstream ss;
        ss << L"["
           << (runtime.portReady ? L"OK" : L"FIX") << L"] Port listening normal\r\n";
        ss << L"  Fix: free the port or change bind/port if needed.\r\n\r\n";
        ss << L"[" << (runtime.localHealthReady ? L"OK" : L"FIX") << L"] Local Host opens\r\n";
        ss << L"  Fix: start service, then test Host URL in browser.\r\n\r\n";
        ss << L"[" << (runtime.embeddedHostReady ? L"OK" : L"FIX") << L"] WebView2 available\r\n";
        ss << L"  Fix: install/repair WebView2 Runtime or use browser fallback.\r\n\r\n";
        ss << L"[" << ((certInfo.certExists && certInfo.keyExists) ? L"OK" : L"FIX") << L"] Certificate ready\r\n";
        ss << L"  Fix: start service once to generate cert files.\r\n\r\n";
        ss << L"[" << (!m_hostIp.empty() ? L"OK" : L"FIX") << L"] LAN IP determined\r\n";
        ss << L"  Fix: re-detect network or choose main IP manually.\r\n\r\n";
        ss << L"[" << (m_shareCardExported ? L"OK" : L"FIX") << L"] Share bundle exported\r\n";
        ss << L"  Fix: export bundle or open Sharing Center.";
        SetWindowTextW(m_diagChecklistCard, ss.str().c_str());
    }
    if (m_diagActionsCard) {
        std::wstringstream ss;
        ss << L"Operator Actions\r\n\r\n";
        ss << L"1. Confirm guest and host are on the same subnet.\r\n";
        ss << L"2. If Viewer fails, paste the Viewer URL directly into a browser.\r\n";
        ss << L"3. If hotspot fails, open system hotspot settings and start it manually.\r\n";
        ss << L"4. If host preview is unavailable, open Host in the system browser.";
        SetWindowTextW(m_diagActionsCard, ss.str().c_str());
    }
    if (m_diagExportCard) {
        std::wstringstream ss;
        ss << L"Export Actions\r\n\r\n";
        ss << L"Output Dir\r\n" << (AppDir() / L"out" / L"share_bundle").wstring() << L"\r\n\r\n";
        ss << L"Use buttons on the right to open, copy, refresh, and export.";
        SetWindowTextW(m_diagExportCard, ss.str().c_str());
    }
    if (m_diagFilesCard) {
        SetWindowTextW(m_diagFilesCard,
            L"Exported Files\r\n\r\nshare_card.html\r\nshare_wizard.html\r\nshare_bundle.json\r\nshare_status.js\r\nshare_diagnostics.txt\r\ndesktop_self_check.html\r\ndesktop_self_check.txt");
    }
    RefreshFilteredLogs();
}

void MainWindow::RefreshSettingsPage() {
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const auto serverExe = AppDir() / L"lan_screenshare_server.exe";
    const auto wwwDir = AppDir() / L"www";
    const auto certDir = AppDir() / L"cert";
    const auto bundleDir = AppDir() / L"out" / L"share_bundle";

    if (m_settingsIntro) {
        std::wstringstream ss;
        ss << L"Settings stays outside the session workflow. Use it to review default policies before operators start a room.\r\n";
        ss << L"Current page is a product-style settings center, not a persisted config backend yet.";
        SetWindowTextW(m_settingsIntro, ss.str().c_str());
    }

    if (m_settingsGeneralCard) {
        std::wstringstream ss;
        ss << L"General\r\n\r\n";
        ss << L"Default Port: " << m_defaultPort << L"\r\n";
        ss << L"Default Bind: " << m_defaultBindAddress << L"\r\n";
        ss << L"Room Rule: " << m_roomRule << L"\r\n";
        ss << L"Token Rule: " << m_tokenRule << L"\r\n\r\n";
        ss << L"Current Session\r\n";
        ss << L"Room: " << m_room << L"\r\n";
        ss << L"Token: " << m_token;
        SetWindowTextW(m_settingsGeneralCard, ss.str().c_str());
    }

    if (m_settingsServiceCard) {
        std::wstringstream ss;
        ss << L"Service\r\n\r\n";
        ss << L"Server EXE\r\n" << m_defaultServerExePath << L"\r\n\r\n";
        ss << L"WWW Dir\r\n" << m_defaultWwwPath << L"\r\n\r\n";
        ss << L"Cert Dir\r\n" << m_defaultCertDir << L"\r\n\r\n";
        ss << L"Args Template\r\n" << m_defaultLaunchArgs;
        SetWindowTextW(m_settingsServiceCard, ss.str().c_str());
    }

    if (m_settingsNetworkCard) {
        std::wstringstream ss;
        ss << L"Network\r\n\r\n";
        ss << L"Main IP Strategy: " << m_defaultIpStrategy << L"\r\n";
        ss << L"Detect Frequency: " << m_autoDetectFrequencySec << L"s\r\n";
        ss << L"Hotspot SSID: auto session alias\r\n";
        ss << L"Password Rule: " << m_hotspotPasswordRule << L"\r\n\r\n";
        ss << L"Live State\r\n";
        ss << L"Selected IP: " << (m_hostIp.empty() ? L"(not selected)" : m_hostIp) << L"\r\n";
        ss << L"Hotspot: " << m_hotspotStatus;
        SetWindowTextW(m_settingsNetworkCard, ss.str().c_str());
    }

    if (m_settingsSharingCard) {
        std::wstringstream ss;
        ss << L"Sharing\r\n\r\n";
        ss << L"Viewer Open Mode: " << m_defaultViewerOpenMode << L"\r\n";
        ss << L"Auto Copy Link: " << (m_autoCopyViewerLink ? L"enabled" : L"disabled") << L"\r\n";
        ss << L"Auto QR: " << (m_autoGenerateQr ? L"enabled" : L"disabled") << L"\r\n";
        ss << L"Auto Bundle Export: " << (m_autoExportBundle ? L"enabled" : L"disabled") << L"\r\n\r\n";
        ss << L"Latest Viewer URL\r\n" << BuildViewerUrl();
        SetWindowTextW(m_settingsSharingCard, ss.str().c_str());
    }

    if (m_settingsLoggingCard) {
        std::wstringstream ss;
        ss << L"Logs & Diagnostics\r\n\r\n";
        ss << L"Log Level: " << m_logLevel << L"\r\n";
        ss << L"Output Dir\r\n" << m_outputDir << L"\r\n\r\n";
        ss << L"Retention Days: " << m_diagnosticsRetentionDays << L"\r\n";
        ss << L"Save stdout/stderr: " << (m_saveStdStreams ? L"yes" : L"no") << L"\r\n\r\n";
        ss << L"Bundle Dir\r\n" << bundleDir.wstring();
        SetWindowTextW(m_settingsLoggingCard, ss.str().c_str());
    }

    if (m_settingsAdvancedCard) {
        std::wstringstream ss;
        ss << L"Advanced\r\n\r\n";
        ss << L"Cert Bypass: " << m_certBypassPolicy << L"\r\n";
        ss << L"WebView Mode: " << m_webViewBehavior << L"\r\n";
        ss << L"Startup Hook: " << m_startupHook << L"\r\n\r\n";
        ss << L"Runtime Flags\r\n";
        ss << L"WebView Ready: " << (runtime.embeddedHostReady ? L"yes" : L"no") << L"\r\n";
        ss << L"Cert Ready: " << ((certInfo.certExists && certInfo.keyExists) ? L"yes" : L"no");
        SetWindowTextW(m_settingsAdvancedCard, ss.str().c_str());
    }

    if (m_settingsCurrentStateCard) {
        std::wstringstream ss;
        ss << L"Current Effective Defaults Snapshot\r\n\r\n";
        ss << L"Default Port -> current port: " << m_defaultPort << L" -> " << m_port << L"\r\n";
        ss << L"Default Bind -> current bind: " << m_defaultBindAddress << L" -> " << m_bindAddress << L"\r\n";
        ss << L"Server Path Exists: " << (fs::exists(serverExe) ? L"yes" : L"no") << L"\r\n";
        ss << L"WWW Path Exists: " << (fs::exists(wwwDir) ? L"yes" : L"no") << L"\r\n";
        ss << L"Cert Dir Exists: " << (fs::exists(certDir) ? L"yes" : L"no") << L"\r\n";
        ss << L"Bundle Dir Exists: " << (fs::exists(bundleDir) ? L"yes" : L"no") << L"\r\n";
        ss << L"Health Ready: " << (runtime.localHealthReady ? L"green / normal" : L"yellow / attention") << L"\r\n";
        ss << L"Host Reachable: " << (runtime.hostIpReachable ? L"green / normal" : L"red / abnormal") << L"\r\n";
        ss << L"Page Role: outside main flow, available for preflight and operator policy review.";
        SetWindowTextW(m_settingsCurrentStateCard, ss.str().c_str());
    }
}

void MainWindow::SelectNetworkCandidate(std::size_t index) {
    if (index >= m_networkCandidateIps.size()) return;
    if (m_networkCandidateIps[index].empty()) return;
    m_hostIp = m_networkCandidateIps[index];
    if (m_ipValue) SetWindowTextW(m_ipValue, m_hostIp.c_str());
    AppendLog(L"Selected main host IP: " + m_hostIp);
    RefreshShareInfo();
    RefreshNetworkPage();
}

void MainWindow::UpdateUiState() {
    const bool running = m_server && m_server->IsRunning();
    if (m_statusText) {
        std::wstring text = L"Status: ";
        text += running ? L"running" : L"stopped";
        SetWindowTextW(m_statusText, text.c_str());
    }
    if (m_webStateText) {
        std::wstring text = L"Host Page: " + m_hostPageState + L" | WebView: " + m_webview.StatusText() + L" | Hotspot: " + m_hotspotStatus;
        SetWindowTextW(m_webStateText, text.c_str());
    }
    if (m_btnStart) EnableWindow(m_btnStart, running ? FALSE : TRUE);
    if (m_btnStop) EnableWindow(m_btnStop, running ? TRUE : FALSE);
    if (m_btnStartHotspot) EnableWindow(m_btnStartHotspot, m_hotspotRunning ? FALSE : TRUE);
    if (m_btnStopHotspot) EnableWindow(m_btnStopHotspot, m_hotspotRunning ? TRUE : FALSE);
    RefreshDashboard();
    RefreshSessionSetup();
    RefreshNetworkPage();
    RefreshSharingPage();
    RefreshMonitorPage();
    RefreshDiagnosticsPage();
    RefreshSettingsPage();
    RefreshHtmlAdminPreview();
}

void MainWindow::OnDestroy() {
    KillTimer(m_hwnd, TIMER_ID);
    StopServer();
}

void MainWindow::StartServer() {
    if (!m_server) return;
    if (m_server->IsRunning()) {
        AppendLog(L"Server already running");
        return;
    }

    wchar_t buf[512]{};
    GetWindowTextW(m_bindEdit, buf, _countof(buf));
    m_bindAddress = buf;

    GetWindowTextW(m_portEdit, buf, _countof(buf));
    m_port = _wtoi(buf);
    if (m_port <= 0) m_port = 9443;

    GetWindowTextW(m_roomEdit, buf, _countof(buf));
    m_room = buf;
    GetWindowTextW(m_tokenEdit, buf, _countof(buf));
    m_token = buf;

    if (m_room.empty() || m_token.empty()) {
        GenerateRoomToken();
    }

    if (m_hostIp.empty()) {
        RefreshHostIp();
    }

    std::wstring portDetail;
    if (!CanBindTcpPort(m_bindAddress, m_port, &portDetail)) {
        AppendLog(L"Start blocked: " + portDetail);
        RefreshShareInfo();
        UpdateUiState();
        return;
    }

    ServerOptions opt;
    fs::path dir = AppDir();
    opt.executable = dir / L"lan_screenshare_server.exe";
    opt.wwwDir = dir / L"www";
    opt.certDir = dir / L"cert";
    opt.bind = m_bindAddress;
    opt.port = std::to_wstring(m_port);

    if (!m_hostIp.empty()) {
        opt.sanIp = m_hostIp + L",127.0.0.1";
    } else {
        opt.sanIp = L"127.0.0.1";
    }

    auto r = m_server->Start(opt);
    if (!r.ok) {
        AppendLog(L"Start failed: " + r.message);
        AddTimelineEvent(L"Service start failed");
    } else {
        AppendLog(L"Server started");
        AddTimelineEvent(L"Service started");
        m_hostPageState = L"loading";
        if (m_currentPage == UiPage::Setup && !PreferHtmlAdminUi()) {
            NavigateHostInWebView();
        }
    }

    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::StopServer() {
    if (!m_server) return;
    if (!m_server->IsRunning()) {
        UpdateUiState();
        return;
    }
    m_server->Stop();
    m_hostPageState = L"stopped";
    AppendLog(L"Server stopped");
    AddTimelineEvent(L"Service stopped");
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::RestartServer() {
    StopServer();
    StartServer();
}

void MainWindow::StartServiceOnly() {
    StartServer();
}

void MainWindow::StartAndOpenHost() {
    StartServer();
    if (m_server && m_server->IsRunning()) {
        OpenHostPage();
    }
}

void MainWindow::RefreshHostIp() {
    lan::network::NetworkInfo info;
    std::string err;
    if (lan::network::NetworkManager::GetCurrentNetworkInfo(info, err) && !info.hostIp.empty()) {
        m_hostIp = urlutil::Utf8ToWide(info.hostIp);
        m_networkMode = info.mode.empty() ? L"lan" : urlutil::Utf8ToWide(info.mode);
        AppendLog(L"Network detected: mode=" + m_networkMode + L", ip=" + m_hostIp);
    } else {
        m_hostIp = DetectBestIPv4();
        m_networkMode = m_hostIp.empty() ? L"unknown" : L"lan";
        if (!err.empty()) {
            AppendLog(L"NetworkManager fallback: " + urlutil::Utf8ToWide(err));
        }
        AppendLog(L"Host IP refreshed: " + (m_hostIp.empty() ? std::wstring(L"(none)") : m_hostIp));
    }

    const auto candidates = CollectActiveIpv4Candidates();
    if (candidates.size() > 1) {
        AppendLog(L"Multiple active IPv4 adapters detected; recommended " + candidates.front().ip + L" on " + candidates.front().name);
    }

    if (m_ipValue) {
        SetWindowTextW(m_ipValue, m_hostIp.empty() ? L"(not found)" : m_hostIp.c_str());
    }
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::RefreshNetworkCapabilities() {
    lan::network::NetworkCapabilities caps;
    std::string err;
    if (lan::network::NetworkManager::QueryCapabilities(caps, err)) {
        m_wifiAdapterPresent = caps.wifiAdapterPresent;
        m_hotspotSupported = caps.hotspotSupported;
        m_wifiDirectApiAvailable = caps.wifiDirectApiAvailable;
    } else if (!err.empty()) {
        AppendLog(L"Capability probe failed: " + urlutil::Utf8ToWide(err));
    }

    if (m_netCapsText) {
        std::wstringstream ss;
        ss << L"Wi-Fi adapter: " << (m_wifiAdapterPresent ? L"present" : L"missing") << L"\r\n";
        ss << L"Hotspot control: " << (m_hotspotSupported ? L"supported / best effort" : L"not detected; use Windows Mobile Hotspot settings") << L"\r\n";
        ss << L"Wi-Fi Direct API: " << (m_wifiDirectApiAvailable ? L"available (pair via Windows UI)" : L"not detected");
        SetWindowTextW(m_netCapsText, ss.str().c_str());
    }

    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::RefreshShareInfo() {
    std::wstringstream ss;
    ss << L"Mode: " << (m_networkMode.empty() ? L"unknown" : m_networkMode) << L"\r\n";
    ss << L"Host IPv4: " << (m_hostIp.empty() ? L"(not found)" : m_hostIp) << L"\r\n";
    ss << L"Port: " << m_port << L"\r\n";
    ss << L"Room: " << m_room << L"\r\n";
    ss << L"Token: " << m_token << L"\r\n";
    ss << L"Host Page: " << m_hostPageState << L"\r\n";
    ss << L"Hotspot: " << m_hotspotStatus << L"\r\n";
    ss << L"SSID: " << (m_hotspotSsid.empty() ? L"(not configured)" : m_hotspotSsid) << L"\r\n";
    ss << L"Password: " << (m_hotspotPassword.empty() ? L"(not configured)" : m_hotspotPassword) << L"\r\n";
    ss << L"Wi-Fi Direct API: " << (m_wifiDirectApiAvailable ? L"available" : L"not detected") << L"\r\n";
    ss << L"Wi-Fi Direct Alias: " << BuildWifiDirectSessionAlias() << L"\r\n";
    const auto certInfo = ProbeCertArtifacts(AppDir());
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, m_bindAddress, m_hostIp, m_port);
    const auto selfCheckReport = BuildSelfCheckReport(m_hostPageState, m_hostIp, BuildViewerUrl(), m_lastViewers,
                                                     runtime.serverProcessRunning,
                                                     certInfo.certExists, certInfo.keyExists,
                                                     runtime.portReady, runtime.portDetail,
                                                     runtime.localHealthReady, runtime.localHealthDetail,
                                                     runtime.hostIpReachable, runtime.hostIpReachableDetail,
                                                     runtime.lanBindReady, runtime.lanBindDetail,
                                                     runtime.activeIpv4Candidates, runtime.selectedIpRecommended, runtime.adapterHint,
                                                     runtime.embeddedHostReady, runtime.embeddedHostStatus,
                                                     m_wifiAdapterPresent, m_hotspotSupported,
                                                     m_wifiDirectApiAvailable, m_hotspotRunning, true);
    ss << L"QR Renderer: local SVG (offline)\r\n";
    ss << L"Cert Files: " << (certInfo.certExists && certInfo.keyExists ? L"ready" : L"missing / incomplete") << L"\r\n";
    ss << L"Port Check: " << (runtime.portReady ? L"ready" : L"attention") << L" (" << runtime.portDetail << L")\r\n";
    ss << L"Local Health: " << (runtime.localHealthReady ? L"ok" : L"attention") << L" (" << runtime.localHealthDetail << L")\r\n";
    ss << L"LAN Bind: " << (runtime.lanBindReady ? L"ready" : L"attention") << L" (" << runtime.lanBindDetail << L")\r\n";
    ss << L"LAN Endpoint: " << (runtime.hostIpReachable ? L"ok" : L"attention") << L" (" << runtime.hostIpReachableDetail << L")\r\n";
    ss << L"Adapter Hint: " << runtime.adapterHint << L"\r\n";
    ss << L"Embedded Host: " << (runtime.embeddedHostReady ? L"ready" : L"attention") << L" (" << runtime.embeddedHostStatus << L")\r\n";
    ss << L"Self-Check: " << BuildSelfCheckSummaryLine(selfCheckReport) << L"\r\n";
    ss << L"Check Categories: cert " << selfCheckReport.certificateCount << L" / net " << selfCheckReport.networkCount << L" / sharing " << selfCheckReport.sharingCount << L"\r\n";
    ss << L"Live Pages: auto refresh from share_status.js\r\n";
    ss << L"Bundle Files: share_card.html / share_wizard.html / desktop_self_check.html / share_bundle.json / share_status.js / share_diagnostics.txt / desktop_self_check.txt\r\n";
    ss << L"Rooms / Viewers: " << m_lastRooms << L" / " << m_lastViewers << L"\r\n\r\n";
    ss << L"Host URL\r\n" << BuildHostUrlLocal() << L"\r\n\r\n";
    ss << L"Viewer URL\r\n" << BuildViewerUrl();

    if (m_shareInfoBox) {
        SetWindowTextW(m_shareInfoBox, ss.str().c_str());
    }
    WriteShareArtifacts(nullptr, nullptr, nullptr, nullptr);
    RefreshDashboard();
    RefreshSessionSetup();
    RefreshNetworkPage();
    RefreshSharingPage();
    RefreshSettingsPage();
    RefreshHtmlAdminPreview();
}

void MainWindow::HandleWebViewMessage(std::wstring_view payload) {
    const std::wstring source = JsonStringField(payload, L"source");
    if (source == L"admin-shell") {
        HandleAdminShellMessage(payload);
        return;
    }

    const std::wstring kind = JsonStringField(payload, L"kind");
    if (kind.empty()) return;

    if (kind == L"status") {
        const std::wstring state = JsonStringField(payload, L"state");
        if (!state.empty()) {
            if (m_hostPageState != state) {
                if (state == L"loading") AddTimelineEvent(L"Host page loading");
                else if (state == L"ready") AddTimelineEvent(L"Host page loaded");
                else if (state == L"sharing") AddTimelineEvent(L"Host page sharing started");
            }
            m_hostPageState = state;
            if (m_webStateText) {
                std::wstring text = L"Host Page: " + m_hostPageState + L" | WebView: " + m_webview.StatusText() + L" | Hotspot: " + m_hotspotStatus;
                SetWindowTextW(m_webStateText, text.c_str());
            }
        }

        std::size_t viewers = 0;
        if (JsonIntField(payload, L"viewers", viewers)) {
            m_lastViewers = viewers;
        }
        RefreshShareInfo();
        return;
    }

    if (kind == L"log") {
        const std::wstring msg = JsonStringField(payload, L"message");
        if (!msg.empty()) {
            AppendLog(L"[host-page] " + msg);
        }
    }
}

void MainWindow::KickPoll() {
    if (!m_server || !m_server->IsRunning()) return;
    if (m_polling.exchange(true)) return;

    int port = m_port;

    std::thread([this, port]() {
        std::wstring url = L"https://127.0.0.1:" + std::to_wstring(port) + L"/api/status";
        HttpResponse r = HttpClient::Get(url, 800);

        size_t rooms = 0, viewers = 0;
        if (r.status == 200) {
            ParseApiStatus(r.body, rooms, viewers);
        }

        auto* pr = new PollResult();
        pr->status = r.status;
        pr->rooms = rooms;
        pr->viewers = viewers;
        PostMessageW(m_hwnd, WM_APP_POLL, (WPARAM)pr, 0);
    }).detach();
}

void MainWindow::HandlePollResult(DWORD status, std::size_t rooms, std::size_t viewers) {
    m_polling.store(false);
    if (!m_statsText) return;

    std::wstringstream ss;
    if (status == 200) {
        if (m_lastViewers == 0 && viewers > 0) {
            AddTimelineEvent(L"First viewer connected");
        } else if (m_lastViewers > 0 && viewers == 0) {
            AddTimelineEvent(L"Viewer disconnected");
        }
        m_lastRooms = rooms;
        m_lastViewers = viewers;
        ss << L"Rooms: " << rooms << L"  Viewers: " << viewers;
    } else {
        ss << L"Rooms: -  Viewers: - (status=" << status << L")";
    }
    SetWindowTextW(m_statsText, ss.str().c_str());
    RefreshShareInfo();
}

std::wstring MainWindow::BuildHostUrlLocal() const {
    std::wstringstream ss;
    std::wstring host = m_hostIp.empty() ? L"127.0.0.1" : m_hostIp;
    ss << L"https://" << host << L":" << m_port << L"/host?room=" << m_room << L"&token=" << m_token;
    return ss.str();
}

std::wstring MainWindow::BuildViewerUrl() const {
    std::wstringstream ss;
    std::wstring host = m_hostIp.empty() ? L"127.0.0.1" : m_hostIp;
    ss << L"https://" << host << L":" << m_port << L"/view?room=" << m_room << L"&token=" << m_token;
    return ss.str();
}

fs::path MainWindow::AppDir() const {
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buf, _countof(buf));
    fs::path p(buf);
    return p.parent_path();
}

fs::path MainWindow::AdminUiDir() const {
    const fs::path runtimeDir = AppDir() / L"webui";
    if (fs::exists(runtimeDir / L"index.html")) {
        return runtimeDir;
    }

    const fs::path sourceDir = AppDir().parent_path().parent_path().parent_path() / L"LanScreenShareHostApp" / L"webui";
    if (fs::exists(sourceDir / L"index.html")) {
        return sourceDir;
    }

    return runtimeDir;
}

bool MainWindow::PreferHtmlAdminUi() const {
    return fs::exists(AdminUiDir() / L"index.html") && m_webview.IsAvailable();
}

bool MainWindow::IsHtmlAdminActive() const {
    return m_webviewMode == WebViewSurfaceMode::HtmlAdminPreview && PreferHtmlAdminUi();
}

std::wstring MainWindow::AdminTabNameForPage(UiPage page) const {
    switch (page) {
    case UiPage::Dashboard:
        return L"dashboard";
    case UiPage::Setup:
        return L"session";
    case UiPage::Network:
        return L"network";
    case UiPage::Sharing:
        return L"sharing";
    case UiPage::Monitor:
        return L"monitor";
    case UiPage::Diagnostics:
        return L"diagnostics";
    case UiPage::Settings:
        return L"settings";
    default:
        return L"dashboard";
    }
}

bool MainWindow::TrySetPageFromAdminTab(std::wstring_view tab) {
    UiPage page = m_currentPage;
    if (tab == L"dashboard") {
        page = UiPage::Dashboard;
    } else if (tab == L"session") {
        page = UiPage::Setup;
    } else if (tab == L"network") {
        page = UiPage::Network;
    } else if (tab == L"sharing") {
        page = UiPage::Sharing;
    } else if (tab == L"monitor") {
        page = UiPage::Monitor;
    } else if (tab == L"diagnostics") {
        page = UiPage::Diagnostics;
    } else if (tab == L"settings") {
        page = UiPage::Settings;
    } else {
        return false;
    }

    if (page == m_currentPage) {
        return false;
    }

    SetPage(page);
    return true;
}

MainWindow* MainWindow::GetInstance(HWND hwnd) {
    return reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    MainWindow* self = nullptr;

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->m_hwnd = hwnd;
        self->OnCreate();
        return 0;
    }

    self = GetInstance(hwnd);

    if (self) {
        switch (msg) {
        case WM_SIZE:
            self->OnSize(LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_COMMAND:
            self->OnCommand(LOWORD(wparam));
            return 0;
        case WM_TIMER:
            if (wparam == TIMER_ID) {
                self->UpdateUiState();
                self->KickPoll();
            }
            return 0;
        case WM_APP_LOG: {
            auto* s = reinterpret_cast<std::wstring*>(wparam);
            if (s) {
                self->AppendLog(*s);
                delete s;
            }
            return 0;
        }
        case WM_APP_POLL: {
            auto* pr = reinterpret_cast<PollResult*>(wparam);
            if (pr) {
                self->HandlePollResult(pr->status, pr->rooms, pr->viewers);
                delete pr;
            }
            return 0;
        }
        case WM_APP_WEBVIEW: {
            auto* s = reinterpret_cast<std::wstring*>(wparam);
            if (s) {
                self->HandleWebViewMessage(*s);
                delete s;
            }
            return 0;
        }
        case WM_DESTROY:
            self->OnDestroy();
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}
