#include "pch.h"
#include "MainWindow.h"
#include "DesktopCommandIds.h"
#include "DesktopHostPageBuilders.h"
#include "ShellEffectExecutor.h"

#include "UrlUtil.h"
#include "HttpClient.h"
#include "../core/cert/cert_manager.h"
#include "../core/runtime/runtime_controller.h"
#include "../core/runtime/share_artifact_service.h"
#include "../core/runtime/desktop_runtime_snapshot.h"
#include "../core/runtime/desktop_shell_presenter.h"
#include "../core/runtime/desktop_layout_presenter.h"
#include "../core/runtime/shell_chrome_presenter.h"
#include "../core/runtime/admin_shell_runtime_publisher.h"
#include "../core/runtime/host_runtime_scheduler.h"
#include "../core/runtime/network_diagnostics_policy.h"
#include "../core/runtime/remote_probe_orchestrator.h"
#include "../platform/abstraction/factory.h"
#include "../platform/host_runtime_refresh_pipeline.h"
#include "../platform/windows/firewall_diagnostics_win.h"
#include "../core/runtime/host_runtime_coordinator.h"

#define WIN32_LEAN_AND_MEAN
#include <iphlpapi.h>
#include <shellapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

const wchar_t CLASS_NAME[] = L"LanScreenShareHostApp";
const UINT WM_APP_LOG = WM_APP + 1;
const UINT WM_APP_POLL = WM_APP + 2;
const UINT WM_APP_WEBVIEW = WM_APP + 3;
const UINT WM_APP_TRAY_RESHOW = WM_APP + 5;



struct PollResult {
    DWORD status = 0;
    size_t rooms = 0;
    size_t viewers = 0;
};

struct AdapterCandidate {
    std::wstring adapterName;
    std::wstring ip;
    std::wstring adapterType;
    bool recommended = false;
    int score = 0;
};

static void ApplyRectToWindow(HWND hwnd, const lan::runtime::DesktopLayoutRect& rect) {
    if (!hwnd) return;
    MoveWindow(hwnd, rect.x, rect.y, rect.width, rect.height, TRUE);
}

static void SetWindowVisible(HWND hwnd, bool visible) {
    if (!hwnd) return;
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

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

static bool FileExistsW(std::wstring_view path) {
    const DWORD attr = GetFileAttributesW(std::wstring(path).c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring QuoteProcessArg(std::wstring_view value) {
    std::wstring out;
    out.reserve(value.size() + 2);
    out.push_back(L'"');
    for (wchar_t ch : value) {
        if (ch == L'"') {
            out.push_back(L'\\');
        }
        out.push_back(ch);
    }
    out.push_back(L'"');
    return out;
}

static std::wstring FindChromiumAppBrowser() {
    auto searchExecutable = [](const wchar_t* exeName) -> std::wstring {
        wchar_t buf[MAX_PATH]{};
        const DWORD len = SearchPathW(nullptr, exeName, nullptr, _countof(buf), buf, nullptr);
        if (len > 0 && len < _countof(buf)) {
            return buf;
        }
        return L"";
    };

    const std::array<const wchar_t*, 4> exeNames = {
        L"msedge.exe",
        L"chrome.exe",
        L"brave.exe",
        L"vivaldi.exe"
    };
    for (const auto* exeName : exeNames) {
        const auto found = searchExecutable(exeName);
        if (!found.empty()) return found;
    }

    wchar_t pf86[MAX_PATH]{};
    wchar_t pf[MAX_PATH]{};
    wchar_t localAppData[MAX_PATH]{};
    GetEnvironmentVariableW(L"ProgramFiles(x86)", pf86, _countof(pf86));
    GetEnvironmentVariableW(L"ProgramFiles", pf, _countof(pf));
    GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, _countof(localAppData));

    const std::vector<std::wstring> candidates = {
        std::wstring(pf86) + L"\\Microsoft\\Edge\\Application\\msedge.exe",
        std::wstring(pf) + L"\\Microsoft\\Edge\\Application\\msedge.exe",
        std::wstring(localAppData) + L"\\Microsoft\\Edge\\Application\\msedge.exe",
        std::wstring(pf86) + L"\\Google\\Chrome\\Application\\chrome.exe",
        std::wstring(pf) + L"\\Google\\Chrome\\Application\\chrome.exe",
        std::wstring(localAppData) + L"\\Google\\Chrome\\Application\\chrome.exe",
        std::wstring(localAppData) + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe",
        std::wstring(pf) + L"\\BraveSoftware\\Brave-Browser\\Application\\brave.exe"
    };
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && FileExistsW(candidate)) {
            return candidate;
        }
    }

    return L"";
}

static bool LaunchUrlInAppWindow(std::wstring_view url, std::wstring* browserPath = nullptr) {
    const auto browser = FindChromiumAppBrowser();
    if (browser.empty()) return false;

    std::wstring cmd = QuoteProcessArg(browser) + L" --app=" + QuoteProcessArg(url);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    const BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (browserPath) *browserPath = browser;
    return true;
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

static std::wstring GuessAdapterType(const IP_ADAPTER_ADDRESSES* adapter, std::wstring_view friendlyName) {
    std::wstring lowered(friendlyName);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](wchar_t c) {
        return static_cast<wchar_t>(::towlower(c));
    });
    if (adapter && adapter->IfType == IF_TYPE_IEEE80211) return L"Wi-Fi";
    if (lowered.find(L"hotspot") != std::wstring::npos || lowered.find(L"hosted") != std::wstring::npos) {
        return L"Hotspot / Hosted";
    }
    if (lowered.find(L"ethernet") != std::wstring::npos) return L"Ethernet";
    if (lowered.find(L"virtual") != std::wstring::npos || lowered.find(L"hyper-v") != std::wstring::npos ||
        lowered.find(L"vmware") != std::wstring::npos || lowered.find(L"wsl") != std::wstring::npos) {
        return L"Virtual / Overlay";
    }
    return L"Ethernet / Other";
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
            candidate.adapterName = friendly;
            candidate.ip = candidateIp;
            candidate.adapterType = GuessAdapterType(a, friendly);
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
        return a.adapterName < b.adapterName;
    });

    std::vector<AdapterCandidate> unique;
    for (const auto& item : out) {
        const bool exists = std::any_of(unique.begin(), unique.end(), [&](const AdapterCandidate& v) {
            return v.ip == item.ip;
        });
        if (!exists) unique.push_back(item);
    }
    for (std::size_t i = 0; i < unique.size(); ++i) {
        unique[i].recommended = (i == 0);
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
    bool ready = false;
    std::wstring detail;
    std::wstring expectedSans;
    std::wstring missingSans;
};

static std::wstring LocalComputerHostName() {
    wchar_t buf[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD size = _countof(buf);
    if (GetComputerNameW(buf, &size) && size > 0) {
        return buf;
    }
    return L"";
}

static std::wstring JoinWideValues(const std::vector<std::wstring>& values) {
    std::wstringstream ss;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) ss << L", ";
        ss << values[i];
    }
    return ss.str();
}

static std::wstring BuildExpectedCertSans(std::wstring_view hostIp) {
    std::vector<std::wstring> entries;
    auto addUnique = [&](std::wstring value) {
        if (value.empty()) return;
        if (std::find(entries.begin(), entries.end(), value) == entries.end()) {
            entries.push_back(std::move(value));
        }
    };

    const std::wstring host(hostIp);
    if (!host.empty() && host != L"(not found)" && host != L"0.0.0.0") {
        addUnique(host);
    }
    addUnique(L"127.0.0.1");
    addUnique(L"localhost");
    addUnique(LocalComputerHostName());
    return JoinWideValues(entries);
}

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
    bool firewallReady = false;
    std::wstring firewallDetail;
    int activeIpv4Candidates = 0;
    bool selectedIpRecommended = true;
    std::wstring adapterHint;
    std::vector<lan::runtime::RemoteProbeCandidateInput> remoteProbeCandidates;
};

static CertArtifactsInfo ProbeCertArtifacts(const fs::path& appDir, std::wstring_view hostIp) {
    CertArtifactsInfo info;
    info.certDir = appDir / L"cert";
    info.certFile = info.certDir / L"server.crt";
    info.keyFile = info.certDir / L"server.key";
    info.certExists = fs::exists(info.certFile);
    info.keyExists = fs::exists(info.keyFile);
    info.expectedSans = BuildExpectedCertSans(hostIp);

    lan::cert::CertStatus status;
    std::string err;
    if (lan::cert::CertManager::InspectCertificate(urlutil::WideToUtf8(info.certFile.wstring()),
                                                   urlutil::WideToUtf8(info.keyFile.wstring()),
                                                   urlutil::WideToUtf8(info.expectedSans),
                                                   status,
                                                   err)) {
        info.certExists = status.certExists;
        info.keyExists = status.keyExists;
        info.ready = status.ready;
        info.detail = urlutil::Utf8ToWide(status.detail);
        if (!status.missingAltNames.empty()) {
            std::vector<std::wstring> missing;
            missing.reserve(status.missingAltNames.size());
            for (const auto& item : status.missingAltNames) {
                missing.push_back(urlutil::Utf8ToWide(item));
            }
            info.missingSans = JoinWideValues(missing);
        }
    } else {
        info.ready = false;
        info.detail = urlutil::Utf8ToWide(err.empty() ? "Certificate inspection failed." : err);
    }
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

static fs::path ResolveHelperScript(const fs::path& appDir, const wchar_t* name) {
    std::vector<fs::path> candidates = {
        appDir / L"scripts" / L"windows" / name,
        appDir.parent_path() / L"scripts" / L"windows" / name,
        appDir.parent_path().parent_path() / L"scripts" / L"windows" / name,
        appDir.parent_path().parent_path().parent_path() / L"scripts" / L"windows" / name,
    };
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && fs::exists(candidate)) return candidate;
    }
    return {};
}

static bool LaunchPowerShellScript(const fs::path& scriptPath,
                                   const std::vector<std::wstring>& args,
                                   std::wstring* err) {
    if (scriptPath.empty() || !fs::exists(scriptPath)) {
        if (err) *err = L"PowerShell helper script is missing.";
        return false;
    }

    std::wstring commandLine = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File " + QuoteProcessArg(scriptPath.wstring());
    for (const auto& arg : args) {
        commandLine += L" " + QuoteProcessArg(arg);
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi)) {
        if (err) *err = L"CreateProcessW failed while starting PowerShell diagnostics helper.";
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
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

static lan::runtime::RemoteProbeCandidateInput ProbeCandidateHealth(const AdapterCandidate& candidate,
                                                                     int port,
                                                                     bool shouldProbe,
                                                                     DWORD timeoutMs = 350) {
    lan::runtime::RemoteProbeCandidateInput probe;
    probe.name = candidate.adapterName;
    probe.ip = candidate.ip;
    probe.type = candidate.adapterType;
    probe.recommended = candidate.recommended;
    probe.probeReady = false;

    if (!shouldProbe) {
        probe.probeDetail = L"Candidate probe was skipped because the local /health path is not ready yet.";
        return probe;
    }

    const std::wstring url = L"https://" + candidate.ip + L":" + std::to_wstring(port) + L"/health";
    const HttpResponse health = HttpClient::Get(url, timeoutMs);
    probe.probeReady = health.status == 200 && health.body.find("ok") != std::string::npos;
    if (probe.probeReady) {
        probe.probeDetail = L"LAN /health probe succeeded for " + candidate.ip + L".";
    } else if (!health.error.empty()) {
        probe.probeDetail = L"LAN /health probe failed for " + candidate.ip + L": " + health.error;
    } else if (health.status != 0) {
        probe.probeDetail = L"LAN /health probe for " + candidate.ip + L" returned HTTP " + std::to_wstring(health.status) + L".";
    } else {
        probe.probeDetail = L"LAN /health probe for " + candidate.ip + L" did not return a usable response.";
    }
    return probe;
}

static RuntimeDiagnosticsSnapshot CollectRuntimeDiagnostics(const ServerController* server,
                                                           const WebViewHost& webview,
                                                           const fs::path& serverExePath,
                                                           std::wstring_view bindAddress,
                                                           std::wstring_view hostIp,
                                                           int port) {
    RuntimeDiagnosticsSnapshot snapshot;
    const auto candidates = CollectActiveIpv4Candidates();
    snapshot.serverProcessRunning = server && server->IsRunning();
    snapshot.embeddedHostReady = webview.IsReady();
    snapshot.embeddedHostStatus = webview.StatusText();

    snapshot.activeIpv4Candidates = static_cast<int>(candidates.size());
    if (candidates.empty()) {
        snapshot.adapterHint = L"No active IPv4 LAN candidate was detected.";
    } else if (candidates.size() == 1) {
        snapshot.adapterHint = L"Single active IPv4 candidate: " + candidates.front().ip + L" on " + candidates.front().adapterName + L".";
    } else {
        snapshot.adapterHint = L"Multiple active IPv4 candidates detected. Recommended: " + candidates.front().ip +
                               L" on " + candidates.front().adapterName + L".";
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

    bool selectedCandidateSeen = false;
    for (std::size_t i = 0; i < candidates.size() && i < 4; ++i) {
        auto probe = ProbeCandidateHealth(candidates[i], port,
                                          snapshot.serverProcessRunning && snapshot.localHealthReady && snapshot.lanBindReady,
                                          candidates[i].recommended ? 450 : 300);
        probe.selected = !hostIp.empty() && probe.ip == hostIp;
        if (probe.selected) {
            selectedCandidateSeen = true;
            snapshot.hostIpReachable = probe.probeReady;
            snapshot.hostIpReachableDetail = probe.probeDetail;
        }
        snapshot.remoteProbeCandidates.push_back(std::move(probe));
    }

    if (!selectedCandidateSeen && snapshot.serverProcessRunning && !hostIp.empty() && hostIp != L"(not found)" && hostIp != L"0.0.0.0") {
        AdapterCandidate selectedCandidate;
        selectedCandidate.adapterName = L"Selected Host IP";
        selectedCandidate.adapterType = L"Manual / Current";
        selectedCandidate.ip = std::wstring(hostIp);
        auto probe = ProbeCandidateHealth(selectedCandidate, port,
                                          snapshot.localHealthReady && snapshot.lanBindReady,
                                          450);
        probe.selected = true;
        snapshot.hostIpReachable = probe.probeReady;
        snapshot.hostIpReachableDetail = probe.probeDetail;
        snapshot.remoteProbeCandidates.push_back(std::move(probe));
    } else if (!selectedCandidateSeen && (hostIp.empty() || hostIp == L"(not found)" || hostIp == L"0.0.0.0")) {
        snapshot.hostIpReachable = false;
        snapshot.hostIpReachableDetail = L"Selected host IP is missing, so LAN endpoint reachability was not probed.";
    } else if (!selectedCandidateSeen && !snapshot.serverProcessRunning) {
        snapshot.hostIpReachable = false;
        snapshot.hostIpReachableDetail = L"Server is not running, so LAN endpoint reachability was not probed.";
    }

    const auto remotePlan = lan::runtime::BuildRemoteProbePlan(snapshot.remoteProbeCandidates);
    if (!remotePlan.detail.empty()) {
        snapshot.adapterHint = remotePlan.detail;
        if (!remotePlan.action.empty()) {
            snapshot.adapterHint += L" " + remotePlan.action;
        }
    }
    if (!snapshot.hostIpReachable && !remotePlan.detail.empty()) {
        snapshot.hostIpReachableDetail = remotePlan.detail;
    }

    if (snapshot.serverProcessRunning) {
        const auto firewall = lan::platform::windows::ProbeFirewallReadiness(serverExePath, port);
        snapshot.firewallReady = firewall.ready;
        snapshot.firewallDetail = firewall.detail;
    } else {
        snapshot.firewallReady = false;
        snapshot.firewallDetail = L"The local server is not running, so firewall readiness was not checked yet.";
    }

    return snapshot;
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

} // namespace

MainWindow::MainWindow()
    : m_platformServices(lan::platform::CreateDefaultPlatformServiceFacade()) {
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
    ApplyHostShellLifecyclePlan(lan::runtime::CoordinateHostShellLifecycle(
        BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::ShowRequested)));
}

void MainWindow::Hide() {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, SW_HIDE);
}

lan::runtime::ShellChromeStateInput MainWindow::BuildShellChromeStateInput() const {
    lan::runtime::ShellChromeStateInput input;
    input.serverRunning = m_server && m_server->IsRunning();
    input.hotspotRunning = m_hotspotRunning;
    input.trayBalloonPending = !m_trayBalloonShown;
    input.hostStateSharing = IsHostStateSharing(m_hostPageState);
    input.attentionNeeded = !m_lastErrorSummary.empty() || m_hostIp.empty();
    input.viewerUrlAvailable = !BuildViewerUrl().empty();
    input.shareActionsAvailable = !m_room.empty() && !m_token.empty();
    input.viewerCount = m_lastViewers;
    input.hostPageState = m_hostPageState;
    input.webviewStatus = m_webview.StatusText();
    input.hotspotStatus = m_hotspotStatus;
    return input;
}

void MainWindow::ApplyShellChromeStatusViewModel(const lan::runtime::ShellChromeStatusViewModel& viewModel) {
    if (m_statusText) {
        SetWindowTextW(m_statusText, viewModel.statusText.c_str());
    }
    if (m_webStateText) {
        SetWindowTextW(m_webStateText, viewModel.webStateText.c_str());
    }
}

lan::runtime::HostShellLifecycleInput MainWindow::BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent event,
                                                                                     bool showBalloon) const {
    const auto schedulerConfig = lan::runtime::DefaultHostRuntimeSchedulerConfig();
    lan::runtime::HostShellLifecycleInput input;
    input.event = event;
    input.exitRequested = m_exitRequested;
    input.showBalloon = showBalloon;
    input.timerIntervalMs = schedulerConfig.tickIntervalMs;
    return input;
}

void MainWindow::ApplyHostShellLifecyclePlan(const lan::runtime::HostShellLifecyclePlan& plan) {
    lan::desktop::ShellEffectExecutor::ApplyHostShellLifecyclePlan(*this, plan);
}

void MainWindow::ExecuteTrayShellCommand(const lan::runtime::TrayShellCommandRoute& route) {
    lan::desktop::ShellEffectExecutor::ExecuteTrayShellCommand(*this, route);
}

void MainWindow::CreateTrayIcon() {
    lan::desktop::ShellEffectExecutor::CreateTrayIcon(*this);
}

void MainWindow::RemoveTrayIcon() {
    lan::desktop::ShellEffectExecutor::RemoveTrayIcon(*this);
}

void MainWindow::UpdateTrayIcon() {
    lan::desktop::ShellEffectExecutor::UpdateTrayIcon(*this);
}

void MainWindow::ShowTrayMenu() {
    lan::desktop::ShellEffectExecutor::ShowTrayMenu(*this);
}

void MainWindow::MinimizeToTray(bool showBalloon) {
    if (!m_hwnd) return;
    ApplyHostShellLifecyclePlan(lan::runtime::CoordinateHostShellLifecycle(
        BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::MinimizeRequested, showBalloon)));
}

void MainWindow::RestoreFromTray() {
    if (!m_hwnd) return;
    ApplyHostShellLifecyclePlan(lan::runtime::CoordinateHostShellLifecycle(
        BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::RestoreRequested)));
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
        m_adminBackend->SetHandlers(BuildAdminShellCoordinatorHooks());
    }

    EnsureHotspotDefaults();
    GenerateRoomToken();
    m_defaultServerExePath = (AppDir() / L"lan_screenshare_server.exe").wstring();
    m_defaultWwwPath = (AppDir() / L"www").wstring();
    m_defaultCertDir = (AppDir() / L"cert").wstring();
    m_outputDir = (AppDir() / L"out").wstring();

    DesktopHostPageBuilders::BuildAll(*this);

    ApplyHostShellLifecyclePlan(lan::runtime::CoordinateHostShellLifecycle(
        BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::StartupReady)));
}

void MainWindow::OnSize(int width, int height) {
    using lan::runtime::DesktopLayoutSurfaceMode;

    DesktopLayoutSurfaceMode surfaceMode = DesktopLayoutSurfaceMode::Hidden;
    switch (m_webviewMode) {
    case WebViewSurfaceMode::HtmlAdminPreview:
        surfaceMode = DesktopLayoutSurfaceMode::HtmlAdminPreview;
        break;
    case WebViewSurfaceMode::HostPreview:
        surfaceMode = DesktopLayoutSurfaceMode::HostPreview;
        break;
    case WebViewSurfaceMode::Hidden:
    default:
        surfaceMode = DesktopLayoutSurfaceMode::Hidden;
        break;
    }

    const auto geometry = lan::runtime::BuildDesktopLayoutGeometry(width, height, surfaceMode);

    RECT webview{};
    webview.left = geometry.webview.x;
    webview.top = geometry.webview.y;
    webview.right = geometry.webview.x + geometry.webview.width;
    webview.bottom = geometry.webview.y + geometry.webview.height;
    m_webview.Resize(webview);

    ApplyRectToWindow(m_shellFallbackBox, geometry.shellFallbackBox);
    ApplyRectToWindow(m_shellRetryBtn, geometry.shellRetryButton);
    ApplyRectToWindow(m_shellStartBtn, geometry.shellStartButton);
    ApplyRectToWindow(m_shellStartHostBtn, geometry.shellStartHostButton);
    ApplyRectToWindow(m_shellOpenHostBtn, geometry.shellOpenHostButton);

    RefreshShellFallback();
}

void MainWindow::SetPage(UiPage page) {
    m_currentPage = page;

    const auto layoutInput = BuildDesktopLayoutStateInput();
    const auto surfaceMode = lan::runtime::ResolveDesktopLayoutSurfaceMode(layoutInput);

    switch (surfaceMode) {
    case lan::runtime::DesktopLayoutSurfaceMode::HtmlAdminPreview:
        if (!m_htmlAdminNavigated || m_webviewMode != WebViewSurfaceMode::HtmlAdminPreview) {
            NavigateHtmlAdminInWebView();
        } else {
            auto state = BuildWebViewShellState();
            state.surface = lan::desktop::WebViewShellSurface::HtmlAdminPreview;
            ApplyWebViewShellState(state);
        }
        break;
    case lan::runtime::DesktopLayoutSurfaceMode::HostPreview:
        NavigateHostInWebView();
        break;
    case lan::runtime::DesktopLayoutSurfaceMode::Hidden:
    default: {
        auto state = BuildWebViewShellState();
        state.surface = lan::desktop::WebViewShellSurface::Hidden;
        ApplyWebViewShellState(state);
        break;
    }
    }

    UpdatePageVisibility();

    RECT rc{};
    GetClientRect(m_hwnd, &rc);
    OnSize(rc.right, rc.bottom);
    RefreshHtmlAdminPreview();
    RefreshShellFallback();
}

lan::runtime::DesktopLayoutStateInput MainWindow::BuildDesktopLayoutStateInput() const {
    lan::runtime::DesktopLayoutStateInput input;
    switch (m_currentPage) {
    case UiPage::Dashboard:
        input.currentPage = lan::runtime::DesktopLayoutPage::Dashboard;
        break;
    case UiPage::Setup:
        input.currentPage = lan::runtime::DesktopLayoutPage::Setup;
        break;
    case UiPage::Network:
        input.currentPage = lan::runtime::DesktopLayoutPage::Network;
        break;
    case UiPage::Sharing:
        input.currentPage = lan::runtime::DesktopLayoutPage::Sharing;
        break;
    case UiPage::Monitor:
        input.currentPage = lan::runtime::DesktopLayoutPage::Monitor;
        break;
    case UiPage::Diagnostics:
        input.currentPage = lan::runtime::DesktopLayoutPage::Diagnostics;
        break;
    case UiPage::Settings:
        input.currentPage = lan::runtime::DesktopLayoutPage::Settings;
        break;
    }
    input.preferHtmlAdminUi = PreferHtmlAdminUi();
    input.serverRunning = m_server && m_server->IsRunning();
    input.webviewReady = m_webview.IsReady();
    return input;
}

void MainWindow::ApplyDesktopPageVisibility(const lan::runtime::DesktopPageVisibility& visibility) {
    auto applyList = [](std::initializer_list<HWND> handles, bool show) {
        for (HWND hwnd : handles) {
            SetWindowVisible(hwnd, show);
        }
    };

    applyList({m_navDashboardBtn, m_navSetupBtn, m_navNetworkBtn, m_navSharingBtn, m_navMonitorBtn, m_navDiagnosticsBtn, m_navSettingsBtn}, visibility.showNativeNavigation);

    applyList({m_dashboardIntro, m_dashboardStatusCard, m_dashboardPrimaryBtn, m_dashboardContinueBtn, m_dashboardWizardBtn, m_dashboardNetworkCard, m_dashboardServiceCard, m_dashboardShareCard, m_dashboardHealthCard, m_dashboardSuggestionsLabel,
               m_dashboardSuggestionText[0], m_dashboardSuggestionText[1], m_dashboardSuggestionText[2], m_dashboardSuggestionText[3],
               m_dashboardSuggestionFixBtn[0], m_dashboardSuggestionFixBtn[1], m_dashboardSuggestionFixBtn[2], m_dashboardSuggestionFixBtn[3],
               m_dashboardSuggestionInfoBtn[0], m_dashboardSuggestionInfoBtn[1], m_dashboardSuggestionInfoBtn[2], m_dashboardSuggestionInfoBtn[3],
               m_dashboardSuggestionSetupBtn[0], m_dashboardSuggestionSetupBtn[1], m_dashboardSuggestionSetupBtn[2], m_dashboardSuggestionSetupBtn[3]}, visibility.showDashboardPage);

    applyList({m_stepInfo, m_setupTitle, m_sessionGroup, m_serviceGroup, m_templateLabel, m_templateCombo, m_ipLabel, m_ipValue, m_btnRefreshIp, m_netCapsText, m_hotspotLabel,
               m_bindLabel, m_bindEdit, m_sanIpLabel, m_sanIpValue, m_advancedToggle, m_portLabel, m_portEdit, m_roomLabel, m_roomEdit, m_tokenLabel, m_tokenEdit, m_btnGenerate,
               m_btnStart, m_btnStop, m_btnRestart, m_btnServiceOnly, m_btnStartAndOpenHost, m_statusText, m_statsText, m_webStateText, m_diagSummaryLabel, m_diagSummaryBox,
               m_firstActionsLabel, m_firstActionsBox, m_shareInfoLabel, m_shareInfoBox, m_sessionSummaryLabel, m_sessionSummaryBox, m_btnOpenViewer, m_btnCopyViewer, m_btnShowQr,
               m_btnShowWizard, m_btnExportBundle, m_btnRunSelfCheck, m_btnRefreshChecks, m_btnOpenDiagnostics, m_btnOpenFolder, m_logLabel, m_logBox, m_runtimeInfoCard}, visibility.showSetupPage);

    applyList({m_networkTitle, m_networkSummaryCard, m_btnRefreshNetwork, m_btnManualSelectIp, m_adapterListLabel,
               m_networkAdapterCards[0], m_networkAdapterCards[1], m_networkAdapterCards[2], m_networkAdapterCards[3],
               m_networkAdapterSelectBtns[0], m_networkAdapterSelectBtns[1], m_networkAdapterSelectBtns[2], m_networkAdapterSelectBtns[3],
               m_hotspotGroup, m_hotspotStatusCard, m_hotspotSsidLabel, m_hotspotPwdLabel, m_hotspotSsidEdit, m_hotspotPwdEdit, m_btnAutoHotspot, m_btnStartHotspot, m_btnStopHotspot,
               m_btnOpenHotspotSettings, m_wifiDirectGroup, m_wifiDirectCard, m_btnPairWifiDirect, m_btnOpenConnectedDevices, m_btnOpenPairingHelp}, visibility.showNetworkPage);

    applyList({m_sharingTitle, m_accessEntryCard, m_qrCard, m_accessGuideCard, m_btnCopyHostUrl, m_btnCopyViewerUrl, m_btnOpenHostBrowser, m_btnOpenViewerBrowser, m_btnSaveQrImage,
               m_btnFullscreenQr, m_btnOpenShareCard, m_btnOpenShareWizard, m_btnOpenBundleFolder, m_btnExportOfflineZip}, visibility.showSharingPage);

    applyList({m_monitorTitle, m_monitorMetricCards[0], m_monitorMetricCards[1], m_monitorMetricCards[2], m_monitorMetricCards[3], m_monitorMetricCards[4], m_monitorTimelineLabel,
               m_monitorTimelineBox, m_monitorTabHealth, m_monitorTabConnections, m_monitorTabLogs, m_monitorDetailBox}, visibility.showMonitorPage);

    applyList({m_diagPageTitle, m_diagChecklistCard, m_diagActionsCard, m_diagExportCard, m_diagFilesCard, m_diagLogSearch, m_diagLevelFilter, m_diagSourceFilter, m_diagLogViewer,
               m_btnDiagOpenOutput, m_btnDiagOpenReport, m_btnDiagExportZip, m_btnDiagCopyPath, m_btnDiagRefreshBundle, m_btnDiagCopyLogs, m_btnDiagSaveLogs}, visibility.showDiagnosticsPage);

    applyList({m_settingsTitle, m_settingsIntro, m_settingsGeneralCard, m_settingsServiceCard, m_settingsNetworkCard, m_settingsSharingCard, m_settingsLoggingCard, m_settingsAdvancedCard,
               m_settingsCurrentStateCard}, visibility.showSettingsPage);

    SetWindowVisible(m_hostPreviewPlaceholder, visibility.showHostPreviewPlaceholder);
    SetWindowVisible(m_btnOpenHost, visibility.showOpenHostButton);
}

void MainWindow::UpdatePageVisibility() {
    ApplyDesktopPageVisibility(lan::runtime::BuildDesktopPageVisibility(BuildDesktopLayoutStateInput()));
    RefreshShellFallback();
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
        RefreshHostRuntime();
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

lan::runtime::HostObservabilityState MainWindow::BuildHostObservabilityState() const {
    lan::runtime::HostObservabilityState state;
    state.logText = m_logs;
    state.timelineText = m_timelineText;
    state.lastErrorSummary = m_lastErrorSummary;
    state.hostPageState = m_hostPageState;
    state.captureState = m_captureState;
    state.captureLabel = m_captureLabel;
    state.lastRooms = m_lastRooms;
    state.lastViewers = m_lastViewers;
    state.handoffDelivered = m_handoffDelivered;
    state.logEntries = m_logEntries;
    return state;
}

void MainWindow::ApplyHostObservabilityState(const lan::runtime::HostObservabilityState& state) {
    m_logs = state.logText;
    m_timelineText = state.timelineText;
    m_lastErrorSummary = state.lastErrorSummary;
    m_hostPageState = state.hostPageState;
    m_captureState = state.captureState;
    m_captureLabel = state.captureLabel;
    m_lastRooms = state.lastRooms;
    m_lastViewers = state.lastViewers;
    m_handoffDelivered = state.handoffDelivered;
    m_logEntries = state.logEntries;
}

void MainWindow::AddTimelineEvent(std::wstring_view eventText) {
    const auto result = lan::runtime::AppendHostObservabilityTimelineEvent(BuildHostObservabilityState(), NowTs(), eventText);
    ApplyHostObservabilityState(result.state);
}


void MainWindow::ExecuteDesktopShellCommand(const lan::runtime::DesktopShellCommandRoute& route) {
    lan::desktop::ShellEffectExecutor::ExecuteDesktopShellCommand(*this, route);
}

void MainWindow::OnCommand(int id) {
    const auto route = lan::runtime::ResolveDesktopShellCommand(id);
    if (route.handled) {
        ExecuteDesktopShellCommand(route);
        return;
    }

    const auto trayRoute = lan::runtime::ResolveTrayShellCommand(id);
    if (trayRoute.handled) {
        ExecuteTrayShellCommand(trayRoute);
        return;
    }
}


void MainWindow::EnsureHotspotDefaults() {
    if (!m_hotspotSsid.empty() && !m_hotspotPassword.empty()) {
        if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
        if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
        return;
    }

    const auto cfg = m_platformServices ? m_platformServices->MakeSuggestedHotspotConfig() : lan::network::HotspotConfig{};
    m_hotspotSsid = urlutil::Utf8ToWide(cfg.ssid);
    m_hotspotPassword = urlutil::Utf8ToWide(cfg.password);
    if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
    if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
}

void MainWindow::GenerateRoomToken() {
    const auto result = lan::runtime::GenerateHostSessionCredentials(BuildHostSessionState());
    ApplyHostSessionState(result.state);
    RefreshShareInfo();
    RefreshSessionSetup();
}

void MainWindow::StartHotspot() {
    wchar_t buf[256]{};
    if (m_hotspotSsidEdit) {
        GetWindowTextW(m_hotspotSsidEdit, buf, _countof(buf));
        m_hotspotSsid = buf;
    }
    if (m_hotspotPwdEdit) {
        GetWindowTextW(m_hotspotPwdEdit, buf, _countof(buf));
        m_hotspotPassword = buf;
    }
    if (m_hotspotSsid.empty() || m_hotspotPassword.empty()) {
        EnsureHotspotDefaults();
    }

    lan::network::HotspotConfig cfg;
    cfg.ssid = urlutil::WideToUtf8(m_hotspotSsid);
    cfg.password = urlutil::WideToUtf8(m_hotspotPassword);

    lan::network::HotspotState out;
    std::string err;
    if (m_platformServices && m_platformServices->StartHotspot(cfg, out, err)) {
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
        if (!m_hotspotSupported) {
            m_hotspotStatus = L"system settings required";
        } else {
            m_hotspotStatus = L"start failed";
        }
        AppendLog(L"Start hotspot failed: " + urlutil::Utf8ToWide(err));
        AddTimelineEvent(L"Hotspot start failed");
    }
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::StopHotspot() {
    std::string err;
    if (m_platformServices && m_platformServices->StopHotspot(err)) {
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
    std::string err;
    if (m_platformServices && m_platformServices->OpenWifiDirectPairing(err)) {
        AppendLog(L"Opened Wi-Fi Direct pairing entry");
    } else {
        AppendLog(L"Open Wi-Fi Direct pairing failed: " + urlutil::Utf8ToWide(err));
    }
}

void MainWindow::OpenSystemHotspotSettings() {
    std::string err;
    if (m_platformServices && m_platformServices->OpenSystemHotspotSettings(err)) {
        AppendLog(L"Opened Windows Mobile Hotspot settings");
    } else {
        AppendLog(L"Open hotspot settings failed: " + urlutil::Utf8ToWide(err));
    }
}

void MainWindow::OpenFirewallSettings() {
    std::string err;
    if (m_platformServices && m_platformServices->OpenFirewallSettings(err)) {
        AppendLog(L"Opened Windows Firewall settings");
    } else {
        AppendLog(L"Open firewall settings failed: " + urlutil::Utf8ToWide(err));
    }
}

void MainWindow::RunNetworkDiagnostics() {
    const auto scriptPath = ResolveHelperScript(AppDir(), L"Run-NetworkDiagnostics.ps1");
    if (scriptPath.empty()) {
        AppendLog(L"Network diagnostics helper is missing");
        MessageBoxW(m_hwnd,
                    L"The Windows network diagnostics helper script could not be found beside this build.\r\n\r\nOpen the scripts/windows folder from the repository or restore the packaging payload before retrying.",
                    L"Network Diagnostics",
                    MB_OK | MB_ICONWARNING);
        return;
    }

    const fs::path outputDir = AppDir() / L"out" / L"diagnostics";
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path reportPath = outputDir / (L"network_diagnostics_" + std::to_wstring(stamp) + L".txt");

    std::vector<std::wstring> args;
    args.push_back(L"-HostIp");
    args.push_back(m_hostIp.empty() ? L"(not found)" : m_hostIp);
    args.push_back(L"-Port");
    args.push_back(std::to_wstring(m_port));
    args.push_back(L"-ServerExe");
    args.push_back((AppDir() / L"lan_screenshare_server.exe").wstring());
    args.push_back(L"-OutputPath");
    args.push_back(reportPath.wstring());

    std::wstring err;
    if (LaunchPowerShellScript(scriptPath, args, &err)) {
        AppendLog(L"Opened network diagnostics helper");
        SetPage(UiPage::Diagnostics);
    } else {
        AppendLog(L"Run network diagnostics failed: " + err);
        MessageBoxW(m_hwnd, err.c_str(), L"Network Diagnostics", MB_OK | MB_ICONERROR);
    }
}


void MainWindow::CheckWebViewRuntime() {
    const auto scriptPath = ResolveHelperScript(AppDir(), L"Check-WebView2Runtime.ps1");
    const fs::path outputDir = AppDir() / L"out" / L"diagnostics";
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path reportPath = outputDir / (L"webview2_runtime_" + std::to_wstring(stamp) + L".txt");

    std::vector<std::wstring> args;
    args.push_back(L"-OutputPath");
    args.push_back(reportPath.wstring());

    std::wstring err;
    if (LaunchPowerShellScript(scriptPath, args, &err)) {
        AppendLog(L"Opened WebView2 runtime helper");
        SetPage(UiPage::Diagnostics);
    } else {
        AppendLog(L"Check WebView2 runtime failed: " + err);
        MessageBoxW(m_hwnd, err.c_str(), L"WebView2 Runtime", MB_OK | MB_ICONERROR);
    }
}

void MainWindow::TrustLocalCertificate() {
    const auto scriptPath = ResolveHelperScript(AppDir(), L"Trust-LocalCertificate.ps1");
    const fs::path certPath = AppDir() / L"cert" / L"server.crt";
    std::vector<std::wstring> args;
    args.push_back(L"-CertPath");
    args.push_back(certPath.wstring());

    std::wstring err;
    if (LaunchPowerShellScript(scriptPath, args, &err)) {
        AppendLog(L"Opened local certificate trust helper");
        SetPage(UiPage::Diagnostics);
    } else {
        AppendLog(L"Trust local certificate failed: " + err);
        MessageBoxW(m_hwnd, err.c_str(), L"Local Certificate", MB_OK | MB_ICONERROR);
    }
}

void MainWindow::ExportRemoteProbeGuide() {
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, AppDir() / L"lan_screenshare_server.exe", m_bindAddress, m_hostIp, m_port);
    const auto plan = lan::runtime::BuildRemoteProbePlan(runtime.remoteProbeCandidates);
    const fs::path outputDir = AppDir() / L"out" / L"diagnostics";
    std::error_code ec;
    fs::create_directories(outputDir, ec);
    const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
    const fs::path guidePath = outputDir / (L"remote_probe_guide_" + std::to_wstring(stamp) + L".txt");

    std::ofstream out(guidePath, std::ios::binary);
    if (!out) {
        AppendLog(L"Remote probe guide export failed");
        MessageBoxW(m_hwnd,
                    L"The remote probe guide could not be written to the diagnostics output folder.",
                    L"Remote Probe Guide",
                    MB_OK | MB_ICONERROR);
        return;
    }

    out << "LAN Screen Share remote probe guide\r\n";
    out << "Generated: " << urlutil::WideToUtf8(NowDateTime()) << "\r\n\r\n";
    out << "Summary: " << urlutil::WideToUtf8(plan.label.empty() ? L"(none)" : plan.label) << "\r\n";
    out << "Detail: " << urlutil::WideToUtf8(plan.detail.empty() ? L"(none)" : plan.detail) << "\r\n";
    out << "Action: " << urlutil::WideToUtf8(plan.action.empty() ? L"(none)" : plan.action) << "\r\n\r\n";
    out << "Current host IP: " << urlutil::WideToUtf8(m_hostIp.empty() ? L"(not found)" : m_hostIp) << "\r\n";
    out << "Viewer URL: " << urlutil::WideToUtf8(BuildViewerUrl()) << "\r\n\r\n";
    out << "Candidate LAN paths\r\n";
    for (const auto& candidate : plan.candidates) {
        out << "- " << urlutil::WideToUtf8(candidate.name) << " [" << urlutil::WideToUtf8(candidate.type) << "]\r\n";
        out << "  IP: " << urlutil::WideToUtf8(candidate.ip) << "\r\n";
        out << "  Flags: recommended=" << (candidate.recommended ? "yes" : "no")
            << ", selected=" << (candidate.selected ? "yes" : "no")
            << ", probe=" << (candidate.probeReady ? "ok" : "failed") << "\r\n";
        out << "  Probe: " << urlutil::WideToUtf8(candidate.probeDetail) << "\r\n";
        out << "  Test URL: https://" << urlutil::WideToUtf8(candidate.ip) << ":" << m_port << "/health\r\n\r\n";
    }
    out << "Remote-device checklist\r\n";
    out << "1. Keep the viewer device on the same LAN / hotspot as the host.\r\n";
    out << "2. Open the Test URL for the selected or recommended candidate in the remote browser first.\r\n";
    out << "3. Accept the local certificate warning for this session if the browser prompts.\r\n";
    out << "4. Once /health answers with ok, open the Viewer URL from the same remote device.\r\n";
    out << "5. If the selected path fails but a recommended candidate answers, switch the main host IP and refresh the share material.\r\n";
    out.close();

    AppendLog(L"Exported remote probe guide: " + guidePath.wstring());
    ShellExecuteW(m_hwnd, L"open", guidePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    SetPage(UiPage::Diagnostics);
}

void MainWindow::OpenHostPage() {
    ExecuteHostAction(lan::runtime::HostActionKind::OpenHostPage);
}

void MainWindow::OpenViewerPage() {
    ExecuteHostAction(lan::runtime::HostActionKind::OpenViewerPage);
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
    m_handoffStarted = true;
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
    ExecuteHostAction(lan::runtime::HostActionKind::ShowQr);
}

void MainWindow::ShowShareWizard() {
    ExecuteHostAction(lan::runtime::HostActionKind::ShowShareWizard);
}

void MainWindow::ExportShareBundle() {
    ExecuteHostAction(lan::runtime::HostActionKind::ExportShareBundle);
}

void MainWindow::QuickFixNetwork() {
    RefreshHostRuntime();
    SetPage(UiPage::Network);
    RefreshDiagnosticsBundle();
    AppendLog(L"Quick fix: refreshed network path");
}

void MainWindow::QuickFixCertificate() {
    RefreshDiagnosticsBundle();
    TrustLocalCertificate();
    OpenDiagnosticsReport();
    SetPage(UiPage::Diagnostics);
    AppendLog(L"Quick fix: opened diagnostics for certificate / trust issues");
}

void MainWindow::QuickFixSharing() {
    StartAndOpenHost();
    SetPage(UiPage::Setup);
    AppendLog(L"Quick fix: started sharing flow");
}

void MainWindow::QuickFixHandoff() {
    if (!BuildViewerUrl().empty()) {
        CopyViewerUrl();
        ShowQr();
        SetPage(UiPage::Sharing);
    } else {
        ShowShareWizard();
    }
    AppendLog(L"Quick fix: refreshed handoff materials");
}

void MainWindow::QuickFixHotspot() {
    if (m_hotspotSupported) {
        StartHotspot();
    } else {
        OpenSystemHotspotSettings();
    }
    SetPage(UiPage::Network);
    AppendLog(L"Quick fix: hotspot fallback path opened");
}

void MainWindow::RunDesktopSelfCheck() {
    ExecuteHostAction(lan::runtime::HostActionKind::RunDesktopSelfCheck);
}

void MainWindow::RefreshDiagnosticsBundle() {
    ExecuteHostAction(lan::runtime::HostActionKind::RefreshDiagnosticsBundle);
}

void MainWindow::OpenDiagnosticsReport() {
    ExecuteHostAction(lan::runtime::HostActionKind::OpenDiagnosticsReport);
}

void MainWindow::OpenOutputFolder() {
    ExecuteHostAction(lan::runtime::HostActionKind::OpenOutputFolder);
}

lan::runtime::DesktopRuntimeSnapshot MainWindow::BuildDesktopRuntimeSnapshot(bool liveReady) const {
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, AppDir() / L"lan_screenshare_server.exe", m_bindAddress, m_hostIp, m_port);
    const auto certInfo = ProbeCertArtifacts(AppDir(), m_hostIp);

    lan::runtime::DesktopRuntimeSnapshotInput input;
    input.networkMode = m_networkMode;
    input.hostIp = m_hostIp;
    input.bindAddress = m_bindAddress;
    input.port = m_port;
    input.room = m_room;
    input.token = m_token;
    input.hostPageState = m_hostPageState;
    input.captureState = m_captureState;
    input.captureLabel = m_captureLabel;
    input.hotspotStatus = m_hotspotStatus;
    input.hotspotSsid = m_hotspotSsid;
    input.hotspotPassword = m_hotspotPassword;
    input.wifiDirectAlias = BuildWifiDirectSessionAlias();
    input.webviewStatusText = m_webview.StatusText();
    input.wifiDirectApiAvailable = m_wifiDirectApiAvailable;
    input.wifiAdapterPresent = m_wifiAdapterPresent;
    input.hotspotSupported = m_hotspotSupported;
    input.hotspotRunning = m_hotspotRunning;
    input.viewerUrlCopied = m_viewerUrlCopied;
    input.shareCardExported = m_shareCardExported;
    input.shareWizardOpened = m_shareWizardOpened;
    input.handoffStarted = m_handoffStarted;
    input.handoffDelivered = m_handoffDelivered;
    input.lastRooms = m_lastRooms;
    input.lastViewers = m_lastViewers;

    input.serverProcessRunning = runtime.serverProcessRunning;
    input.certReady = certInfo.ready;
    input.certDetail = certInfo.detail;
    input.expectedSans = certInfo.expectedSans;
    input.portReady = runtime.portReady;
    input.portDetail = runtime.portDetail;
    input.localHealthReady = runtime.localHealthReady;
    input.localHealthDetail = runtime.localHealthDetail;
    input.hostIpReachable = runtime.hostIpReachable;
    input.hostIpReachableDetail = runtime.hostIpReachableDetail;
    input.lanBindReady = runtime.lanBindReady;
    input.lanBindDetail = runtime.lanBindDetail;
    input.activeIpv4Candidates = runtime.activeIpv4Candidates;
    input.selectedIpRecommended = runtime.selectedIpRecommended;
    input.adapterHint = runtime.adapterHint;
    input.embeddedHostReady = runtime.embeddedHostReady;
    input.embeddedHostStatus = runtime.embeddedHostStatus;
    input.firewallReady = runtime.firewallReady;
    input.firewallDetail = runtime.firewallDetail;
    input.liveReady = liveReady;

    return lan::runtime::BuildDesktopRuntimeSnapshot(input);
}

bool MainWindow::WriteShareArtifacts(fs::path* shareCardPath,
                                     fs::path* shareWizardPath,
                                     fs::path* bundleJsonPath,
                                     fs::path* desktopSelfCheckPath) {
    const fs::path outDir = AppDir() / L"out" / L"share_bundle";
    const auto generatedAt = NowDateTime();
    const auto certInfo = ProbeCertArtifacts(AppDir(), m_hostIp);
    const auto snapshot = BuildDesktopRuntimeSnapshot(true);

    lan::runtime::ShareArtifactWriteRequest request;
    request.outputDir = outDir;
    request.qrAssetSource = AppDir() / L"www" / L"assets" / L"share_card_qr.bundle.js";
    request.session = snapshot.session;
    request.health = snapshot.health;
    request.cert.certDir = certInfo.certDir;
    request.cert.certFile = certInfo.certFile;
    request.cert.keyFile = certInfo.keyFile;
    request.cert.certExists = certInfo.certExists;
    request.cert.keyExists = certInfo.keyExists;
    request.cert.ready = certInfo.ready;
    request.cert.detail = certInfo.detail;
    request.cert.expectedSans = certInfo.expectedSans;
    request.cert.missingSans = certInfo.missingSans;
    request.generatedAt = generatedAt;
    request.liveReady = true;

    lan::runtime::ShareArtifactWriteResult result;
    if (!lan::runtime::ExportShareArtifacts(request, &result)) {
        AppendLog(result.errorMessage.empty() ? L"Export share artifacts failed" : result.errorMessage);
        return false;
    }

    if (shareCardPath) *shareCardPath = result.shareCardPath;
    if (shareWizardPath) *shareWizardPath = result.shareWizardPath;
    if (bundleJsonPath) *bundleJsonPath = result.bundleJsonPath;
    if (desktopSelfCheckPath) *desktopSelfCheckPath = result.desktopSelfCheckPath;
    return true;
}

std::wstring MainWindow::BuildWifiDirectSessionAlias() const {
    return L"LanShare-" + (m_room.empty() ? std::wstring(L"session") : m_room);
}

lan::desktop::WebViewShellState MainWindow::BuildWebViewShellState() const {
    lan::desktop::WebViewShellState state;
    switch (m_webviewMode) {
    case WebViewSurfaceMode::HostPreview:
        state.surface = lan::desktop::WebViewShellSurface::HostPreview;
        break;
    case WebViewSurfaceMode::HtmlAdminPreview:
        state.surface = lan::desktop::WebViewShellSurface::HtmlAdminPreview;
        break;
    case WebViewSurfaceMode::Hidden:
    default:
        state.surface = lan::desktop::WebViewShellSurface::Hidden;
        break;
    }
    state.adminShellReady = m_adminShellReady;
    state.htmlAdminNavigated = m_htmlAdminNavigated;
    return state;
}

lan::desktop::WebViewShellContext MainWindow::BuildWebViewShellContext() const {
    lan::desktop::WebViewShellContext context;
    context.parent = m_hwnd;

    RECT rc{};
    if (m_hwnd) {
        GetClientRect(m_hwnd, &rc);
    }
    context.bounds = rc;
    context.hostPreviewUrl = BuildHostUrlLocal();
    context.htmlAdminIndexFile = AdminUiDir() / L"index.html";
    return context;
}

lan::desktop::WebViewShellHooks MainWindow::BuildWebViewShellHooks() {
    lan::desktop::WebViewShellHooks hooks;
    hooks.log = [this](std::wstring msg) {
        if (!m_hwnd) return;
        auto* heap = new std::wstring(std::move(msg));
        PostMessageW(m_hwnd, WM_APP_LOG, (WPARAM)heap, 0);
    };
    hooks.webMessage = [this](std::wstring payload) {
        if (!m_hwnd) return;
        auto* heap = new std::wstring(std::move(payload));
        PostMessageW(m_hwnd, WM_APP_WEBVIEW, (WPARAM)heap, 0);
    };
    return hooks;
}

void MainWindow::ApplyWebViewShellState(const lan::desktop::WebViewShellState& state) {
    switch (state.surface) {
    case lan::desktop::WebViewShellSurface::HostPreview:
        m_webviewMode = WebViewSurfaceMode::HostPreview;
        break;
    case lan::desktop::WebViewShellSurface::HtmlAdminPreview:
        m_webviewMode = WebViewSurfaceMode::HtmlAdminPreview;
        break;
    case lan::desktop::WebViewShellSurface::Hidden:
    default:
        m_webviewMode = WebViewSurfaceMode::Hidden;
        break;
    }
    m_adminShellReady = state.adminShellReady;
    m_htmlAdminNavigated = state.htmlAdminNavigated;
}

void MainWindow::RestoreWebViewShellState() {
    auto state = BuildWebViewShellState();
    const auto context = BuildWebViewShellContext();
    const auto plan = lan::desktop::BuildWebViewRestorePlan(state, context);
    lan::desktop::ApplyWebViewShellPlan(m_webview, state, plan, context, BuildWebViewShellHooks());
    ApplyWebViewShellState(state);
}

void MainWindow::NavigateHostInWebView() {
    auto state = BuildWebViewShellState();
    const auto context = BuildWebViewShellContext();
    const auto plan = lan::desktop::BuildWebViewHostNavigationPlan(state, context);
    lan::desktop::ApplyWebViewShellPlan(m_webview, state, plan, context, BuildWebViewShellHooks());
    ApplyWebViewShellState(state);
}

void MainWindow::NavigateHtmlAdminInWebView() {
    auto state = BuildWebViewShellState();
    const auto context = BuildWebViewShellContext();
    const auto plan = lan::desktop::BuildWebViewHtmlAdminNavigationPlan(state, context);
    lan::desktop::ApplyWebViewShellPlan(m_webview, state, plan, context, BuildWebViewShellHooks());
    ApplyWebViewShellState(state);
}

void MainWindow::EnsureWebViewInitialized() {
    auto state = BuildWebViewShellState();
    lan::desktop::WebViewShellPlan plan;
    plan.nextState = state;
    plan.ensureInitialized = true;
    const auto context = BuildWebViewShellContext();
    lan::desktop::ApplyWebViewShellPlan(m_webview, state, plan, context, BuildWebViewShellHooks());
    ApplyWebViewShellState(state);
}

void MainWindow::PublishAdminShellRuntime() {
    if (!m_adminBackend) return;

    lan::runtime::AdminShellRuntimePublishContext context;
    context.adminShellActive = IsHtmlAdminActive();
    context.adminShellReady = m_adminShellReady;
    context.viewModelInput = BuildAdminViewModelInput();

    lan::runtime::AdminShellRuntimePublisherHooks hooks;
    hooks.publishJson = [this](const std::wstring& json) { m_webview.PostJson(json); };
    lan::runtime::PublishAdminShellRuntime(context, hooks);
}

void MainWindow::RefreshHtmlAdminPreview() {
    PublishAdminShellRuntime();
    RefreshShellFallback();
}

lan::runtime::AdminShellCoordinatorHooks MainWindow::BuildAdminShellCoordinatorHooks() {
    lan::runtime::AdminShellCoordinatorHooks hooks;
    hooks.refreshRuntime = [this]() { RefreshHostRuntime(); };
    hooks.generateRoomToken = [this]() { GenerateRoomToken(); };
    hooks.applySessionConfig = [this](const lan::runtime::AdminShellSessionRequest& request) {
        ApplyAdminShellSessionRequest(request);
    };
    hooks.applyHotspotConfig = [this](const lan::runtime::AdminShellHotspotRequest& request) {
        ApplyAdminShellHotspotRequest(request);
    };
    hooks.executeHostAction = [this](lan::runtime::HostActionKind kind) { ExecuteHostAction(kind); };
    hooks.copyHostUrl = [this]() { CopyHostUrl(); };
    hooks.copyViewerUrl = [this]() { CopyViewerUrl(); };
    hooks.quickFixNetwork = [this]() { QuickFixNetwork(); };
    hooks.quickFixCertificate = [this]() { QuickFixCertificate(); };
    hooks.quickFixSharing = [this]() { QuickFixSharing(); };
    hooks.quickFixHandoff = [this]() { QuickFixHandoff(); };
    hooks.quickFixHotspot = [this]() { QuickFixHotspot(); };
    hooks.selectNetworkCandidate = [this](std::size_t index) { SelectNetworkCandidate(index); };
    hooks.startHotspot = [this]() { StartHotspot(); };
    hooks.stopHotspot = [this]() { StopHotspot(); };
    hooks.autoHotspot = [this]() {
        EnsureHotspotDefaults();
        RefreshNetworkPage();
        RefreshHtmlAdminPreview();
    };
    hooks.openHotspotSettings = [this]() { OpenSystemHotspotSettings(); };
    hooks.openFirewallSettings = [this]() { OpenFirewallSettings(); };
    hooks.runNetworkDiagnostics = [this]() { RunNetworkDiagnostics(); };
    hooks.checkWebViewRuntime = [this]() { CheckWebViewRuntime(); };
    hooks.trustLocalCertificate = [this]() { TrustLocalCertificate(); };
    hooks.exportRemoteProbeGuide = [this]() { ExportRemoteProbeGuide(); };
    hooks.openConnectedDevices = [this]() { OpenWifiDirectPairing(); };
    hooks.navigatePage = [this](std::wstring page) { TrySetPageFromAdminTab(page); };
    return hooks;
}

void MainWindow::HandleAdminShellMessage(std::wstring_view payload) {
    if (!m_adminBackend) return;

    const auto result = m_adminBackend->HandleMessage(payload);
    if (!result.logLine.empty()) {
        AppendLog(result.logLine);
    }

    const auto policy = lan::runtime::ResolveAdminShellRuntimeRefreshPolicy(result.requestSnapshot, result.stateChanged);
    if (policy.markShellReady) {
        m_adminShellReady = true;
    }
    if (policy.shouldPublish) {
        PublishAdminShellRuntime();
    }
    RefreshShellFallback();
}


lan::runtime::AdminViewModelInput MainWindow::BuildAdminViewModelInput() const {
    const auto certInfo = ProbeCertArtifacts(AppDir(), m_hostIp);
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, AppDir() / L"lan_screenshare_server.exe", m_bindAddress, m_hostIp, m_port);
    const auto runtimeSnapshot = BuildDesktopRuntimeSnapshot(true);
    const auto sessionModel = lan::runtime::BuildHostSessionAdminModel(BuildHostSessionState());
    const auto serverExe = AppDir() / L"lan_screenshare_server.exe";
    const auto wwwDir = AppDir() / L"www";
    const auto certDir = AppDir() / L"cert";
    const auto bundleDir = AppDir() / L"out" / L"share_bundle";

    lan::runtime::AdminViewModelInput input;
    input.appName = L"LanScreenShareHostApp";
    input.nativePage = AdminTabNameForPage(m_currentPage);
    input.runtimeSnapshot = runtimeSnapshot;
    input.sessionModel = sessionModel;
    input.lastErrorSummary = m_lastErrorSummary;
    input.outputDir = (AppDir() / L"out").wstring();
    input.bundleDir = bundleDir.wstring();
    input.serverExePath = serverExe.wstring();
    input.certDir = certInfo.certDir.wstring();
    input.timelineText = m_timelineText;
    input.logTail = m_logs.size() > 8000 ? m_logs.substr(m_logs.size() - 8000) : m_logs;
    input.defaultServerExePath = m_defaultServerExePath;
    input.defaultWwwPath = m_defaultWwwPath;
    input.defaultCertDir = m_defaultCertDir;
    input.defaultLaunchArgs = m_defaultLaunchArgs;
    input.defaultIpStrategy = m_defaultIpStrategy;
    input.autoDetectFrequencySec = m_autoDetectFrequencySec;
    input.hotspotPasswordRule = m_hotspotPasswordRule;
    input.logLevel = m_logLevel;
    input.configuredDefaultViewerOpenMode = m_defaultViewerOpenMode;
    input.outputDirSetting = m_outputDir;
    input.diagnosticsRetentionDays = m_diagnosticsRetentionDays;
    input.certBypassPolicy = m_certBypassPolicy;
    input.snapshotWebViewBehavior = PreferHtmlAdminUi() ? L"html-admin" : L"embedded-host";
    input.configuredWebViewBehavior = m_webViewBehavior;
    input.snapshotStartupHook = L"none";
    input.configuredStartupHook = m_startupHook;
    input.autoCopyViewerLink = m_autoCopyViewerLink;
    input.autoGenerateQr = m_autoGenerateQr;
    input.autoExportBundle = m_autoExportBundle;
    input.saveStdStreams = m_saveStdStreams;
    input.serverExeExists = fs::exists(serverExe);
    input.wwwDirExists = fs::exists(wwwDir);
    input.certDirExists = fs::exists(certDir);
    input.bundleDirExists = fs::exists(bundleDir);

    input.networkCandidates.reserve(runtime.remoteProbeCandidates.size());
    for (const auto& candidate : runtime.remoteProbeCandidates) {
        lan::runtime::AdminViewNetworkCandidate view;
        view.name = candidate.name;
        view.ip = candidate.ip;
        view.type = candidate.type;
        view.recommended = candidate.recommended;
        view.selected = candidate.selected;
        view.probeReady = candidate.probeReady;
        view.probeLabel = candidate.probeReady ? L"LAN /health ok" : L"LAN /health failed";
        view.probeDetail = candidate.probeDetail;
        input.networkCandidates.push_back(std::move(view));
    }

    return input;
}

void MainWindow::ApplyAdminShellSessionRequest(const lan::runtime::AdminShellSessionRequest& request) {
    const auto result = lan::runtime::ApplyHostSessionConfig(BuildHostSessionState(), request.room, request.token, request.bindAddress, request.port);
    ApplyHostSessionState(result.state);
    AppendLog(L"Admin shell applied session config");
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::ApplyAdminShellHotspotRequest(const lan::runtime::AdminShellHotspotRequest& request) {
    m_hotspotSsid = request.ssid;
    m_hotspotPassword = request.password;
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
    const auto result = lan::runtime::AppendHostObservabilityLog(BuildHostObservabilityState(), NowTs(), line);
    ApplyHostObservabilityState(result.state);
    if (m_logBox) SetWindowTextW(m_logBox, m_logs.c_str());
    if (result.refreshDashboard) RefreshDashboard();
    if (result.refreshDiagnostics) RefreshDiagnosticsPage();
}


void MainWindow::ApplyNativeCommandButtonPolicy(const lan::runtime::NativeCommandButtonPolicy& policy) {
    if (m_btnStart) EnableWindow(m_btnStart, policy.startEnabled ? TRUE : FALSE);
    if (m_btnStop) EnableWindow(m_btnStop, policy.stopEnabled ? TRUE : FALSE);
    if (m_btnStartHotspot) EnableWindow(m_btnStartHotspot, policy.startHotspotEnabled ? TRUE : FALSE);
    if (m_btnStopHotspot) EnableWindow(m_btnStopHotspot, policy.stopHotspotEnabled ? TRUE : FALSE);
    if (m_shellStartBtn) EnableWindow(m_shellStartBtn, policy.shellStartEnabled ? TRUE : FALSE);
}

void MainWindow::ApplyDashboardButtonPolicy(const lan::runtime::DashboardButtonPolicy& policy) {
    if (m_dashboardPrimaryBtn) EnableWindow(m_dashboardPrimaryBtn, policy.primaryActionEnabled ? TRUE : FALSE);
    for (std::size_t slot = 0; slot < m_dashboardSuggestionFixBtn.size(); ++slot) {
        if (m_dashboardSuggestionFixBtn[slot]) EnableWindow(m_dashboardSuggestionFixBtn[slot], policy.suggestionFixEnabled[slot] ? TRUE : FALSE);
        if (m_dashboardSuggestionInfoBtn[slot]) EnableWindow(m_dashboardSuggestionInfoBtn[slot], policy.suggestionInfoEnabled[slot] ? TRUE : FALSE);
        if (m_dashboardSuggestionSetupBtn[slot]) EnableWindow(m_dashboardSuggestionSetupBtn[slot], policy.suggestionSetupEnabled[slot] ? TRUE : FALSE);
    }
}

void MainWindow::ApplyNetworkButtonPolicy(const lan::runtime::NetworkButtonPolicy& policy) {
    for (std::size_t i = 0; i < m_networkAdapterSelectBtns.size(); ++i) {
        if (m_networkAdapterSelectBtns[i]) EnableWindow(m_networkAdapterSelectBtns[i], policy.adapterSelectEnabled[i] ? TRUE : FALSE);
    }
}

void MainWindow::RefreshShellFallback() {
    if (!m_shellFallbackBox || !m_shellRetryBtn || !m_shellStartBtn || !m_shellStartHostBtn || !m_shellOpenHostBtn) return;

    lan::runtime::ShellStateInput input;
    input.htmlAdminMode = m_webviewMode == WebViewSurfaceMode::HtmlAdminPreview;
    input.adminShellReady = m_adminShellReady;
    input.serverRunning = m_server && m_server->IsRunning();
    input.uiBundleExists = fs::exists(AdminUiDir() / L"index.html");
    input.webviewStatus = m_webview.StatusText();
    input.webviewDetail = m_webview.DetailText();
    const auto viewModel = lan::runtime::BuildShellFallbackViewModel(input);

    if (m_hwnd) {
        RECT rc{};
        GetClientRect(m_hwnd, &rc);
        const int width = (rc.right > rc.left) ? static_cast<int>(rc.right - rc.left) : 0;
        const int height = (rc.bottom > rc.top) ? static_cast<int>(rc.bottom - rc.top) : 0;

        if (viewModel.showFallback) {
            RECT hidden{};
            m_webview.Resize(hidden);
        } else if (input.htmlAdminMode) {
            RECT admin{};
            admin.left = 0;
            admin.top = 0;
            admin.right = width;
            admin.bottom = height;
            m_webview.Resize(admin);
        }
    }

    SetWindowVisible(m_shellFallbackBox, viewModel.showFallback);
    SetWindowVisible(m_shellRetryBtn, viewModel.showFallback);
    SetWindowVisible(m_shellStartBtn, viewModel.showFallback);
    SetWindowVisible(m_shellStartHostBtn, viewModel.showFallback);
    SetWindowVisible(m_shellOpenHostBtn, viewModel.showFallback);

    if (viewModel.showFallback) {
        SetWindowPos(m_shellFallbackBox, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(m_shellRetryBtn, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(m_shellStartBtn, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(m_shellStartHostBtn, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        SetWindowPos(m_shellOpenHostBtn, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    SetWindowTextW(m_shellStartBtn, viewModel.startButtonLabel.c_str());
    m_shellStartButtonEnabled = viewModel.startButtonEnabled;
    SetWindowTextW(m_shellStartHostBtn, viewModel.startHostButtonLabel.c_str());
    const auto running = m_server && m_server->IsRunning();
    ApplyNativeCommandButtonPolicy(lan::runtime::BuildNativeCommandButtonPolicy({running, m_hotspotRunning, m_hotspotSupported, m_shellStartButtonEnabled}));

    if (!viewModel.showFallback) {
        return;
    }

    SetWindowTextW(m_shellFallbackBox, viewModel.bodyText.c_str());
}

void MainWindow::RefreshDashboard() {
    if (PreferHtmlAdminUi()) return;

    const auto viewModel = lan::runtime::BuildDashboardViewModel(BuildAdminViewModelInput());

    SetTextIfPresent(m_dashboardStatusCard, viewModel.statusCard);
    SetTextIfPresent(m_dashboardNetworkCard, viewModel.networkCard);
    SetTextIfPresent(m_dashboardServiceCard, viewModel.serviceCard);
    SetTextIfPresent(m_dashboardShareCard, viewModel.shareCard);
    SetTextIfPresent(m_dashboardHealthCard, viewModel.healthCard);

    const auto buttonPolicy = lan::runtime::BuildDashboardButtonPolicy(viewModel, m_dashboardSuggestionKinds.size());

    for (auto& kind : m_dashboardSuggestionKinds) kind = DashboardSuggestionKind::None;
    for (auto& detail : m_dashboardSuggestionDetails) detail.clear();

    std::size_t slot = 0;
    for (const auto& suggestion : viewModel.suggestions) {
        if (slot >= m_dashboardSuggestionKinds.size()) break;
        m_dashboardSuggestionKinds[slot] = suggestion.kind;
        m_dashboardSuggestionDetails[slot] = suggestion.detail;
        SetTextIfPresent(m_dashboardSuggestionText[slot], suggestion.title);
        ++slot;
    }
    while (slot < m_dashboardSuggestionKinds.size()) {
        m_dashboardSuggestionKinds[slot] = DashboardSuggestionKind::OpenDiagnostics;
        m_dashboardSuggestionDetails[slot] = L"Open diagnostics to inspect the full runtime snapshot.";
        SetTextIfPresent(m_dashboardSuggestionText[slot], L"No higher-priority suggestion right now.");
        ++slot;
    }

    ApplyDashboardButtonPolicy(buttonPolicy);
}

void MainWindow::RefreshSessionSetup() {
    if (PreferHtmlAdminUi()) return;
    const auto certInfo = ProbeCertArtifacts(AppDir(), m_hostIp);
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, AppDir() / L"lan_screenshare_server.exe", m_bindAddress, m_hostIp, m_port);
    const std::wstring hostUrl = BuildHostUrlLocal();
    const auto editViewModel = lan::runtime::BuildDesktopEditSessionViewModel(BuildDesktopEditSessionInput());

    if (m_sanIpValue) {
        std::wstring san = BuildExpectedCertSans(m_hostIp);
        SetWindowTextW(m_sanIpValue, san.c_str());
    }

    ApplyDesktopEditSessionViewModel(editViewModel);

    if (m_runtimeInfoCard) {
        std::wstringstream ss;
        ss << L"Runtime Info\r\n";
        ss << L"Uptime: " << (runtime.serverProcessRunning ? L"running" : L"stopped") << L"\r\n";
        ss << L"Rooms: " << m_lastRooms << L"\r\n";
        ss << L"Viewers: " << m_lastViewers << L"\r\n";
        ss << L"Recent Heartbeat: " << (runtime.localHealthReady ? L"/health ok" : runtime.localHealthDetail) << L"\r\n";
        ss << L"Host Ready State: " << m_hostPageState << L"\r\n";
        ss << L"Local Reachability: " << (runtime.hostIpReachable ? L"ok" : runtime.hostIpReachableDetail) << L"\r\n\r\n";
        ss << L"Edit Session State\r\n" << editViewModel.statusText;
        SetWindowTextW(m_runtimeInfoCard, ss.str().c_str());
    }

    if (m_hostPreviewPlaceholder) {
        std::wstringstream ss;
        ss << L"Host Preview Unavailable\r\n\r\n";
        if (runtime.embeddedHostStatus == L"sdk-unavailable") {
            ss << L"Reason: this build was compiled without WebView2 SDK support.\r\n\r\n";
        } else if (runtime.embeddedHostStatus == L"runtime-unavailable" || runtime.embeddedHostStatus == L"controller-unavailable") {
            ss << L"Reason: WebView2 Runtime is unavailable or did not initialize.\r\n\r\n";
        } else {
            ss << L"Reason: Host preview is not ready yet.\r\n\r\n";
        }
        ss << L"Action: Open Host in the system browser.\r\n";
        ss << L"Host URL: " << hostUrl << L"\r\n";
        ss << L"Cert State: " << (certInfo.ready ? L"ready" : L"needs refresh");
        if (!certInfo.detail.empty()) {
            ss << L"\r\nCert Detail: " << certInfo.detail;
        }
        SetWindowTextW(m_hostPreviewPlaceholder, ss.str().c_str());
        const auto layoutVisibility = lan::runtime::BuildDesktopPageVisibility(BuildDesktopLayoutStateInput());
        SetWindowVisible(m_hostPreviewPlaceholder, layoutVisibility.showHostPreviewPlaceholder);
        SetWindowVisible(m_btnOpenHost, layoutVisibility.showOpenHostButton);
    }
}

void MainWindow::RefreshNetworkPage() {
    if (PreferHtmlAdminUi()) return;
    std::array<bool, 4> candidatePresent{};
    const auto candidates = CollectActiveIpv4Candidates();
    const auto runtime = CollectRuntimeDiagnostics(m_server.get(), m_webview, AppDir() / L"lan_screenshare_server.exe", m_bindAddress, m_hostIp, m_port);

    if (m_networkSummaryCard) {
        std::wstringstream ss;
        ss << L"Current Recommended IPv4: " << (candidates.empty() ? L"(none)" : candidates.front().ip) << L"\r\n";
        ss << L"Adapter: " << (candidates.empty() ? L"(none)" : candidates.front().adapterName) << L"\r\n";
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
            const auto probeIt = std::find_if(runtime.remoteProbeCandidates.begin(), runtime.remoteProbeCandidates.end(), [&](const auto& probe) {
                return probe.ip == c.ip;
            });
            const bool recommended = c.recommended;
            const bool selected = c.ip == m_hostIp;
            const bool probeReady = probeIt != runtime.remoteProbeCandidates.end() && probeIt->probeReady;
            const std::wstring probeDetail = probeIt != runtime.remoteProbeCandidates.end()
                ? probeIt->probeDetail
                : L"Probe detail is unavailable for this candidate.";
            m_networkCandidateIps[i] = c.ip;
            ss << L"Adapter: " << c.adapterName << L"\r\n";
            ss << L"IPv4: " << c.ip << L"\r\n";
            ss << L"Type: " << c.adapterType << L" | Online: yes\r\n";
            ss << L"Recommended: " << (recommended ? L"yes" : L"no") << L" | Selected: " << (selected ? L"yes" : L"no") << L"\r\n";
            ss << L"Probe: " << (probeReady ? L"LAN /health ok" : L"needs attention") << L"\r\n";
            ss << probeDetail;
        } else {
            m_networkCandidateIps[i].clear();
            ss << L"No adapter candidate in this slot.";
        }
        if (m_networkAdapterCards[i]) SetWindowTextW(m_networkAdapterCards[i], ss.str().c_str());
        candidatePresent[i] = hasCandidate;
    }

    ApplyNetworkButtonPolicy(lan::runtime::BuildNetworkButtonPolicy(candidatePresent));

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
    if (PreferHtmlAdminUi()) return;
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
    if (PreferHtmlAdminUi()) return;
    const auto viewModel = lan::runtime::BuildMonitorViewModel(BuildAdminViewModelInput());
    for (std::size_t i = 0; i < m_monitorMetricCards.size() && i < viewModel.metricCards.size(); ++i) {
        if (m_monitorMetricCards[i]) SetWindowTextW(m_monitorMetricCards[i], viewModel.metricCards[i].c_str());
    }

    if (m_monitorTimelineBox) {
        SetWindowTextW(m_monitorTimelineBox, viewModel.timelineText.c_str());
    }

    if (m_monitorDetailBox) {
        SetWindowTextW(m_monitorDetailBox, viewModel.detailText.c_str());
    }
}

void MainWindow::RefreshFilteredLogs() {
    if (PreferHtmlAdminUi()) return;
    if (!m_diagLogViewer) return;
    wchar_t searchBuf[256]{};
    if (m_diagLogSearch) GetWindowTextW(m_diagLogSearch, searchBuf, _countof(searchBuf));
    const int levelSel = m_diagLevelFilter ? static_cast<int>(SendMessageW(m_diagLevelFilter, CB_GETCURSEL, 0, 0)) : 0;
    const int sourceSel = m_diagSourceFilter ? static_cast<int>(SendMessageW(m_diagSourceFilter, CB_GETCURSEL, 0, 0)) : 0;

    lan::runtime::HostObservabilityFilter filter;
    filter.searchText = searchBuf;
    filter.levelFilter = levelSel == 1 ? L"Info" : levelSel == 2 ? L"Warning" : levelSel == 3 ? L"Error" : L"";
    filter.sourceFilter = sourceSel == 1 ? L"app" : sourceSel == 2 ? L"network" : sourceSel == 3 ? L"server" : sourceSel == 4 ? L"webview" : L"";

    const auto text = lan::runtime::BuildHostObservabilityFilteredLogText(BuildHostObservabilityState(), filter);
    SetWindowTextW(m_diagLogViewer, text.c_str());
}

void MainWindow::RefreshDiagnosticsPage() {
    if (PreferHtmlAdminUi()) return;
    const auto viewModel = lan::runtime::BuildDiagnosticsViewModel(BuildAdminViewModelInput());
    if (m_diagChecklistCard) {
        SetWindowTextW(m_diagChecklistCard, viewModel.checklistCard.c_str());
    }
    if (m_diagActionsCard) {
        SetWindowTextW(m_diagActionsCard, viewModel.actionsCard.c_str());
    }
    if (m_diagExportCard) {
        SetWindowTextW(m_diagExportCard, viewModel.exportCard.c_str());
    }
    if (m_diagFilesCard) {
        SetWindowTextW(m_diagFilesCard, viewModel.filesCard.c_str());
    }
    RefreshFilteredLogs();
}

void MainWindow::RefreshSettingsPage() {
    if (PreferHtmlAdminUi()) return;
    const auto viewModel = lan::runtime::BuildSettingsViewModel(BuildAdminViewModelInput());

    if (m_settingsIntro) {
        SetWindowTextW(m_settingsIntro, viewModel.intro.c_str());
    }
    if (m_settingsGeneralCard) {
        SetWindowTextW(m_settingsGeneralCard, viewModel.generalCard.c_str());
    }
    if (m_settingsServiceCard) {
        SetWindowTextW(m_settingsServiceCard, viewModel.serviceCard.c_str());
    }
    if (m_settingsNetworkCard) {
        SetWindowTextW(m_settingsNetworkCard, viewModel.networkCard.c_str());
    }
    if (m_settingsSharingCard) {
        SetWindowTextW(m_settingsSharingCard, viewModel.sharingCard.c_str());
    }
    if (m_settingsLoggingCard) {
        SetWindowTextW(m_settingsLoggingCard, viewModel.loggingCard.c_str());
    }
    if (m_settingsAdvancedCard) {
        SetWindowTextW(m_settingsAdvancedCard, viewModel.advancedCard.c_str());
    }
    if (m_settingsCurrentStateCard) {
        SetWindowTextW(m_settingsCurrentStateCard, viewModel.currentStateCard.c_str());
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

void MainWindow::HandleRuntimeTick() {
    lan::runtime::HostRuntimeTickInput input;
    input.serverRunning = m_server && m_server->IsRunning();
    input.pollInFlight = m_polling.load();

    const auto result = lan::runtime::CoordinateHostRuntimeTick(m_runtimeTickState, input);
    m_runtimeTickState = result.nextState;

    if (result.refreshUi) {
        UpdateUiState();
    }
    if (result.kickPoll) {
        KickPoll();
    }
}

void MainWindow::UpdateUiState() {
    const bool running = m_server && m_server->IsRunning();
    ApplyShellChromeStatusViewModel(lan::runtime::BuildShellChromeStatusViewModel(BuildShellChromeStateInput()));
    RefreshDashboard();
    RefreshSessionSetup();
    RefreshNetworkPage();
    RefreshSharingPage();
    RefreshMonitorPage();
    RefreshDiagnosticsPage();
    RefreshSettingsPage();
    RefreshHtmlAdminPreview();
    RefreshShellFallback();
    ApplyNativeCommandButtonPolicy(lan::runtime::BuildNativeCommandButtonPolicy({running, m_hotspotRunning, m_hotspotSupported, m_shellStartButtonEnabled}));
    UpdateTrayIcon();
}

void MainWindow::OnDestroy() {
    ApplyHostShellLifecyclePlan(lan::runtime::CoordinateHostShellLifecycle(
        BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::DestroyRequested)));
}

void MainWindow::ExecuteHostAction(lan::runtime::HostActionKind kind) {
    lan::runtime::HostActionHooks hooks;
    hooks.startServer = [this]() { return PerformStartServerAction(); };
    hooks.stopServer = [this]() { return PerformStopServerAction(); };
    hooks.openHostPage = [this]() { return PerformOpenHostPageAction(); };
    hooks.openViewerPage = [this]() { return PerformOpenViewerPageAction(); };
    hooks.ensureArtifacts = [this](const lan::runtime::HostActionArtifactRequest& request,
                                   lan::runtime::HostActionArtifactPaths& paths) {
        return PerformEnsureShareArtifactsAction(request, paths);
    };
    hooks.openPath = [this](const fs::path& path) { return PerformOpenPathAction(path); };

    ApplyHostActionResult(lan::runtime::ExecuteHostAction(kind, BuildHostActionContext(), hooks));
}

void MainWindow::ApplyHostActionResult(const lan::runtime::HostActionResult& result) {
    if (result.effects.handoffStarted) m_handoffStarted = true;
    if (result.effects.shareWizardOpened) m_shareWizardOpened = true;
    if (result.effects.shareCardExported) m_shareCardExported = true;
    if (result.effects.clearHandoffDelivered) m_handoffDelivered = false;

    if (result.effects.setPage) {
        switch (result.effects.page) {
        case lan::runtime::HostActionPage::Setup:
            SetPage(UiPage::Setup);
            break;
        case lan::runtime::HostActionPage::Network:
            SetPage(UiPage::Network);
            break;
        case lan::runtime::HostActionPage::Sharing:
            SetPage(UiPage::Sharing);
            break;
        case lan::runtime::HostActionPage::Diagnostics:
            SetPage(UiPage::Diagnostics);
            break;
        case lan::runtime::HostActionPage::None:
        default:
            break;
        }
    }

    for (const auto& line : result.logs) {
        AppendLog(line);
    }
    for (const auto& eventText : result.timelineEvents) {
        AddTimelineEvent(eventText);
    }

    if (result.effects.refreshShareInfo) {
        RefreshShareInfo();
    } else if (result.effects.refreshDashboard) {
        RefreshDashboard();
    }
    UpdateUiState();
}

lan::runtime::HostSessionState MainWindow::BuildHostSessionState() const {
    lan::runtime::HostSessionState state;
    state.rules.defaultPort = m_defaultPort;
    state.rules.defaultBindAddress = m_defaultBindAddress;
    state.rules.roomRule = L"letters / digits, 3-32 chars";
    state.rules.tokenRule = L"letters / digits, 6-64 chars";
    state.rules.generatedRoomLength = 6;
    state.rules.generatedTokenLength = 16;

    state.config.bindAddress = m_bindAddress;
    state.config.port = m_port;
    state.config.room = m_room;
    state.config.token = m_token;

    state.flags.viewerUrlCopied = m_viewerUrlCopied;
    state.flags.shareBundleExported = m_shareCardExported;
    state.flags.shareWizardOpened = m_shareWizardOpened;
    state.flags.handoffStarted = m_handoffStarted;
    state.flags.handoffDelivered = m_handoffDelivered;
    return state;
}

void MainWindow::ApplyHostSessionState(const lan::runtime::HostSessionState& state, bool updateControls) {
    const auto normalized = lan::runtime::NormalizeHostSessionState(state);
    m_defaultPort = normalized.rules.defaultPort;
    m_defaultBindAddress = normalized.rules.defaultBindAddress;
    m_roomRule = normalized.rules.roomRule;
    m_tokenRule = normalized.rules.tokenRule;

    m_bindAddress = normalized.config.bindAddress;
    m_port = normalized.config.port;
    m_room = normalized.config.room;
    m_token = normalized.config.token;

    m_viewerUrlCopied = normalized.flags.viewerUrlCopied;
    m_shareCardExported = normalized.flags.shareBundleExported;
    m_shareWizardOpened = normalized.flags.shareWizardOpened;
    m_handoffStarted = normalized.flags.handoffStarted;
    m_handoffDelivered = normalized.flags.handoffDelivered;

    if (updateControls) {
        if (m_bindEdit) SetWindowTextW(m_bindEdit, m_bindAddress.c_str());
        if (m_portEdit) {
            const std::wstring portText = std::to_wstring(m_port);
            SetWindowTextW(m_portEdit, portText.c_str());
        }
        if (m_roomEdit) SetWindowTextW(m_roomEdit, m_room.c_str());
        if (m_tokenEdit) SetWindowTextW(m_tokenEdit, m_token.c_str());
    }
}

lan::runtime::DesktopEditSessionDraft MainWindow::BuildDesktopEditSessionDraftFromControls() const {
    auto draft = lan::runtime::BuildDesktopEditSessionDraft(BuildHostSessionState(),
        m_templateCombo ? static_cast<int>(SendMessageW(m_templateCombo, CB_GETCURSEL, 0, 0)) : 0);

    wchar_t buf[512]{};
    if (m_bindEdit) {
        GetWindowTextW(m_bindEdit, buf, _countof(buf));
        draft.bindAddress = buf;
    }
    if (m_portEdit) {
        GetWindowTextW(m_portEdit, buf, _countof(buf));
        draft.portText = buf;
    }
    if (m_roomEdit) {
        GetWindowTextW(m_roomEdit, buf, _countof(buf));
        draft.room = buf;
    }
    if (m_tokenEdit) {
        GetWindowTextW(m_tokenEdit, buf, _countof(buf));
        draft.token = buf;
    }
    return draft;
}

lan::runtime::DesktopEditSessionInput MainWindow::BuildDesktopEditSessionInput() const {
    lan::runtime::DesktopEditSessionInput input;
    input.appliedState = BuildHostSessionState();
    input.draft = BuildDesktopEditSessionDraftFromControls();
    input.serverRunning = m_server && m_server->IsRunning();
    input.hostIp = m_hostIp;
    input.outputDir = (AppDir() / L"out").wstring();
    input.shareBundleDir = (AppDir() / L"out" / L"share_bundle").wstring();
    return input;
}

void MainWindow::ApplyDesktopEditSessionDraftToControls(const lan::runtime::DesktopEditSessionDraft& draft) {
    if (m_bindEdit) SetWindowTextW(m_bindEdit, draft.bindAddress.c_str());
    if (m_portEdit) SetWindowTextW(m_portEdit, draft.portText.c_str());
    if (m_roomEdit) SetWindowTextW(m_roomEdit, draft.room.c_str());
    if (m_tokenEdit) SetWindowTextW(m_tokenEdit, draft.token.c_str());
    if (m_templateCombo) {
        SendMessageW(m_templateCombo, CB_SETCURSEL, draft.templateIndex, 0);
    }
}

lan::runtime::HostSessionMutationResult MainWindow::SyncHostSessionStateFromControls() {
    const auto draft = BuildDesktopEditSessionDraftFromControls();
    bool portValid = true;
    auto mutation = lan::runtime::ApplyDesktopEditSessionDraft(BuildHostSessionState(), draft, true, &portValid);
    ApplyHostSessionState(mutation.state);
    return mutation;
}

void MainWindow::ApplyDesktopEditSessionViewModel(const lan::runtime::DesktopEditSessionViewModel& viewModel) {
    if (m_sessionSummaryLabel) SetWindowTextW(m_sessionSummaryLabel, viewModel.sessionSummaryLabel.c_str());
    if (m_sessionSummaryBox) SetWindowTextW(m_sessionSummaryBox, viewModel.sessionSummaryBody.c_str());
    if (m_btnStart) SetWindowTextW(m_btnStart, viewModel.startButtonLabel.c_str());
    if (m_btnRestart) SetWindowTextW(m_btnRestart, viewModel.restartButtonLabel.c_str());
    if (m_btnServiceOnly) SetWindowTextW(m_btnServiceOnly, viewModel.serviceOnlyButtonLabel.c_str());
    if (m_btnStartAndOpenHost) SetWindowTextW(m_btnStartAndOpenHost, viewModel.startAndOpenHostButtonLabel.c_str());
    if (m_btnGenerate) EnableWindow(m_btnGenerate, viewModel.buttonPolicy.generateEnabled ? TRUE : FALSE);
    if (m_btnRestart) EnableWindow(m_btnRestart, viewModel.buttonPolicy.restartEnabled ? TRUE : FALSE);
    if (m_btnServiceOnly) EnableWindow(m_btnServiceOnly, viewModel.buttonPolicy.serviceOnlyEnabled ? TRUE : FALSE);
    if (m_btnStartAndOpenHost) EnableWindow(m_btnStartAndOpenHost, viewModel.buttonPolicy.startAndOpenHostEnabled ? TRUE : FALSE);
}

lan::runtime::HostActionContext MainWindow::BuildHostActionContext() const {
    lan::runtime::HostActionContext context;
    context.outputDir = AppDir() / L"out" / L"share_bundle";
    context.diagnosticsReportPath = context.outputDir / L"share_diagnostics.txt";
    context.diagnosticsReportExists = fs::exists(context.diagnosticsReportPath);
    return context;
}

lan::runtime::HostActionOperation MainWindow::PerformStartServerAction() {
    if (!m_server) {
        return {false, false, L"Server controller unavailable"};
    }
    if (m_server->IsRunning()) {
        return {true, false, L"Server already running"};
    }

    SyncHostSessionStateFromControls();
    const auto ensured = lan::runtime::EnsureHostSessionCredentials(BuildHostSessionState());
    ApplyHostSessionState(ensured.state);

    if (m_hostIp.empty()) {
        RefreshHostRuntime();
    }

    std::wstring portDetail;
    if (!CanBindTcpPort(m_bindAddress, m_port, &portDetail)) {
        return {false, true, L"Start blocked: " + portDetail};
    }

    ServerOptions opt;
    fs::path dir = AppDir();
    opt.executable = dir / L"lan_screenshare_server.exe";
    opt.wwwDir = dir / L"www";
    opt.certDir = dir / L"cert";
    opt.bind = m_bindAddress;
    opt.port = std::to_wstring(m_port);
    opt.sanIp = BuildExpectedCertSans(m_hostIp);

    auto r = m_server->Start(opt);
    if (!r.ok) {
        return {false, true, L"Start failed: " + r.message};
    }

    m_hostPageState = L"loading";
    m_captureState = L"idle";
    m_captureLabel.clear();
    if (m_currentPage == UiPage::Setup && !PreferHtmlAdminUi()) {
        NavigateHostInWebView();
    }
    return {true, true, L""};
}

lan::runtime::HostActionOperation MainWindow::PerformStopServerAction() {
    if (!m_server) {
        return {false, false, L"Server controller unavailable"};
    }
    if (!m_server->IsRunning()) {
        return {true, false, L"Server already stopped"};
    }
    m_server->Stop();
    m_hostPageState = L"stopped";
    m_captureState = L"idle";
    m_captureLabel.clear();
    return {true, true, L""};
}

lan::runtime::HostActionOperation MainWindow::PerformOpenHostPageAction() {
    const auto url = BuildHostUrlLocal();
    std::wstring browserPath;
    if (LaunchUrlInAppWindow(url, &browserPath)) {
        return {true, true, L"Opened Host page in app window: " + browserPath};
    }

    std::string err;
    if (m_platformServices && m_platformServices->OpenExternalUrl(urlutil::WideToUtf8(url), err)) {
        return {true, true, L"Opened Host page in browser"};
    }
    return {false, true, L"Open Host page failed: " + urlutil::Utf8ToWide(err)};
}

lan::runtime::HostActionOperation MainWindow::PerformOpenViewerPageAction() {
    const auto url = BuildViewerUrl();
    std::wstring browserPath;
    const bool preferAppWindow = m_defaultViewerOpenMode == L"app-window-preferred" || m_defaultViewerOpenMode == L"app-window";
    if (preferAppWindow && LaunchUrlInAppWindow(url, &browserPath)) {
        return {true, true, L"Opened Viewer page in app window: " + browserPath};
    }

    std::string err;
    if (m_platformServices && m_platformServices->OpenExternalUrl(urlutil::WideToUtf8(url), err)) {
        return {true, true, L"Opened Viewer page in browser"};
    }
    return {false, true, L"Open Viewer page failed: " + urlutil::Utf8ToWide(err)};
}

lan::runtime::HostActionOperation MainWindow::PerformEnsureShareArtifactsAction(const lan::runtime::HostActionArtifactRequest& request,
                                                                                lan::runtime::HostActionArtifactPaths& paths) {
    if (!WriteShareArtifacts(request.shareCard ? &paths.shareCardPath : nullptr,
                             request.shareWizard ? &paths.shareWizardPath : nullptr,
                             request.bundleJson ? &paths.bundleJsonPath : nullptr,
                             request.desktopSelfCheck ? &paths.desktopSelfCheckPath : nullptr)) {
        return {false, true, L"Export share artifacts failed"};
    }
    return {true, true, L""};
}

lan::runtime::HostActionOperation MainWindow::PerformOpenPathAction(const fs::path& path) {
    std::error_code ec;
    if (!path.has_extension()) {
        fs::create_directories(path, ec);
    } else if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
    }

    std::string err;
    if (m_platformServices && m_platformServices->OpenExternalPath(path, err)) {
        if (path.has_extension()) {
            return {true, true, L""};
        }
        return {true, true, L"Opened bundle folder: " + path.wstring()};
    }

    const std::wstring failure = path.has_extension()
        ? (L"Open path failed: " + urlutil::Utf8ToWide(err))
        : (L"Open bundle folder failed: " + urlutil::Utf8ToWide(err));
    return {false, true, failure};
}

void MainWindow::StartServer() {
    ExecuteHostAction(lan::runtime::HostActionKind::StartServer);
}

void MainWindow::StopServer() {
    ExecuteHostAction(lan::runtime::HostActionKind::StopServer);
}

void MainWindow::RestartServer() {
    ExecuteHostAction(lan::runtime::HostActionKind::RestartServer);
}

void MainWindow::StartServiceOnly() {
    ExecuteHostAction(lan::runtime::HostActionKind::StartServiceOnly);
}

void MainWindow::StartAndOpenHost() {
    ExecuteHostAction(lan::runtime::HostActionKind::StartAndOpenHost);
}

void MainWindow::ApplyHostRuntimeRefresh(const lan::runtime::HostRuntimeRefreshResult& refresh) {
    m_hostIp = refresh.hostIp;
    m_networkMode = refresh.networkMode;
    m_hotspotSsid = refresh.hotspotSsid;
    m_hotspotPassword = refresh.hotspotPassword;
    m_hotspotStatus = refresh.hotspotStatus;
    m_hotspotRunning = refresh.hotspotRunning;
    m_wifiAdapterPresent = refresh.wifiAdapterPresent;
    m_hotspotSupported = refresh.hotspotSupported;
    m_wifiDirectApiAvailable = refresh.wifiDirectApiAvailable;

    for (const auto& line : refresh.logLines) {
        AppendLog(line);
    }

    const auto candidates = CollectActiveIpv4Candidates();
    if (candidates.size() > 1) {
        AppendLog(L"Multiple active IPv4 adapters detected; recommended " + candidates.front().ip + L" on " + candidates.front().adapterName);
    }

    if (m_ipValue) {
        SetWindowTextW(m_ipValue, m_hostIp.empty() ? L"(not found)" : m_hostIp.c_str());
    }
    if (m_hotspotSsidEdit) SetWindowTextW(m_hotspotSsidEdit, m_hotspotSsid.c_str());
    if (m_hotspotPwdEdit) SetWindowTextW(m_hotspotPwdEdit, m_hotspotPassword.c_str());
    if (m_netCapsText) {
        SetWindowTextW(m_netCapsText, lan::runtime::BuildNetworkCapabilitiesText(refresh).c_str());
    }
}

void MainWindow::RefreshHostRuntime() {
    lan::runtime::HostRuntimeRefreshInput input;
    input.fallbackHostIp = DetectBestIPv4();
    input.existingNetworkMode = m_networkMode;
    input.existingHotspotSsid = m_hotspotSsid;
    input.existingHotspotPassword = m_hotspotPassword;
    input.existingWifiAdapterPresent = m_wifiAdapterPresent;
    input.existingHotspotSupported = m_hotspotSupported;
    input.existingWifiDirectApiAvailable = m_wifiDirectApiAvailable;

    const auto refresh = lan::platform::RunHostRuntimeRefreshPipeline(m_platformServices.get(), input);
    ApplyHostRuntimeRefresh(refresh);
    RefreshShareInfo();
    UpdateUiState();
}

void MainWindow::RefreshShareInfo() {
    const auto runtimeSnapshot = BuildDesktopRuntimeSnapshot(true);

    if (m_shareInfoBox) {
        const std::wstring shareInfo = lan::runtime::BuildShareInfoText(runtimeSnapshot.session,
                                                                        runtimeSnapshot.health,
                                                                        runtimeSnapshot.selfCheckSummary);
        SetWindowTextW(m_shareInfoBox, shareInfo.c_str());
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
    const auto message = lan::runtime::ParseShellBridgeInboundMessage(payload);
    if (message.source == lan::runtime::ShellBridgeSource::AdminShell) {
        HandleAdminShellMessage(payload);
        return;
    }

    if (message.kind == lan::runtime::ShellBridgeInboundKind::HostStatus) {
        const auto result = lan::runtime::CoordinateHostStatusMessage(BuildHostObservabilityState(), message, NowTs());
        ApplyHostObservabilityState(result.state);
        ApplyShellChromeStatusViewModel(lan::runtime::BuildShellChromeStatusViewModel(BuildShellChromeStateInput()));
        if (result.refreshShareInfo) RefreshShareInfo();
        return;
    }

    if (message.kind == lan::runtime::ShellBridgeInboundKind::HostLog && !message.logMessage.empty()) {
        AppendLog(L"[host-page] " + message.logMessage);
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

    const auto result = lan::runtime::CoordinateHostPollResult(BuildHostObservabilityState(), static_cast<long>(status), rooms, viewers, NowTs());
    ApplyHostObservabilityState(result.state);
    SetWindowTextW(m_statsText, result.statsText.c_str());
    if (result.refreshShareInfo) RefreshShareInfo();
    if (result.updateTrayIcon) UpdateTrayIcon();
}

std::wstring MainWindow::BuildHostUrlLocal() const {
    lan::runtime::RuntimeSessionState sessionState;
    sessionState.hostIp = m_hostIp;
    sessionState.port = m_port;
    sessionState.room = m_room;
    sessionState.token = m_token;
    return lan::runtime::BuildHostUrl(sessionState);
}

std::wstring MainWindow::BuildViewerUrl() const {
    lan::runtime::RuntimeSessionState sessionState;
    sessionState.hostIp = m_hostIp;
    sessionState.port = m_port;
    sessionState.room = m_room;
    sessionState.token = m_token;
    return lan::runtime::BuildViewerUrl(sessionState);
}

void MainWindow::BuildHandoffSummary(std::wstring* state, std::wstring* label, std::wstring* detail) const {
    const auto runtimeSnapshot = BuildDesktopRuntimeSnapshot(true);
    if (state) *state = runtimeSnapshot.handoff.state;
    if (label) *label = runtimeSnapshot.handoff.label;
    if (detail) *detail = runtimeSnapshot.handoff.detail;
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
    return true;
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
            if (wparam == SIZE_MINIMIZED) {
                self->MinimizeToTray(true);
                return 0;
            }
            self->OnSize(LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_COMMAND:
            self->OnCommand(LOWORD(wparam));
            return 0;
        case WM_CLOSE:
            self->ApplyHostShellLifecyclePlan(lan::runtime::CoordinateHostShellLifecycle(
                self->BuildHostShellLifecycleInput(lan::runtime::HostShellLifecycleEvent::CloseRequested, true)));
            return 0;
        case WM_TIMER:
            if (wparam == lan::desktop::kHostRuntimeTimerId) {
                self->HandleRuntimeTick();
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
        case lan::desktop::kHostAppTrayIconMessage:
            switch (LOWORD(lparam)) {
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case NIN_SELECT:
            case NIN_KEYSELECT:
                self->RestoreFromTray();
                return 0;
            case WM_CONTEXTMENU:
            case WM_RBUTTONUP:
                self->ShowTrayMenu();
                return 0;
            default:
                break;
            }
            return 0;
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
