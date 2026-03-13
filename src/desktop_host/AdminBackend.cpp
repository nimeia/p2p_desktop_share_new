#include "pch.h"
#include "AdminBackend.h"

#include "UrlUtil.h"

#include <cstdio>
#include <sstream>

namespace {

std::wstring JsonStringField(std::wstring_view body, std::wstring_view key) {
    std::wstring pattern = L"\"" + std::wstring(key) + L"\"";
    std::size_t pos = body.find(pattern);
    if (pos == std::wstring::npos) return L"";
    pos = body.find(L':', pos + pattern.size());
    if (pos == std::wstring::npos) return L"";
    ++pos;
    while (pos < body.size() && iswspace(body[pos])) ++pos;
    if (pos >= body.size()) return L"";
    if (body[pos] != L'"') return L"";
    ++pos;
    std::wstring value;
    while (pos < body.size()) {
        wchar_t ch = body[pos++];
        if (ch == L'\\' && pos < body.size()) {
            wchar_t next = body[pos++];
            switch (next) {
            case L'"': value.push_back(L'"'); break;
            case L'\\': value.push_back(L'\\'); break;
            case L'/': value.push_back(L'/'); break;
            case L'b': value.push_back(L'\b'); break;
            case L'f': value.push_back(L'\f'); break;
            case L'n': value.push_back(L'\n'); break;
            case L'r': value.push_back(L'\r'); break;
            case L't': value.push_back(L'\t'); break;
            default: value.push_back(next); break;
            }
            continue;
        }
        if (ch == L'"') {
            return value;
        }
        value.push_back(ch);
    }
    return L"";
}

bool JsonIntField(std::wstring_view body, std::wstring_view key, std::size_t& value) {
    std::wstring pattern = L"\"" + std::wstring(key) + L"\"";
    std::size_t pos = body.find(pattern);
    if (pos == std::wstring::npos) return false;
    pos = body.find(L':', pos + pattern.size());
    if (pos == std::wstring::npos) return false;
    ++pos;
    while (pos < body.size() && iswspace(body[pos])) ++pos;
    if (pos >= body.size()) return false;
    std::size_t end = pos;
    while (end < body.size() && iswdigit(body[end])) ++end;
    if (end == pos) return false;
    value = static_cast<std::size_t>(_wtoi(std::wstring(body.substr(pos, end - pos)).c_str()));
    return true;
}

std::string JsonEscapeUtf8(std::string_view value) {
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

void AppendJsonString(std::ostringstream& json, const char* key, std::wstring_view value, bool trailingComma = true) {
    json << "\"" << key << "\":\"" << JsonEscapeUtf8(urlutil::WideToUtf8(std::wstring(value))) << "\"";
    if (trailingComma) json << ",";
}

void AppendJsonBool(std::ostringstream& json, const char* key, bool value, bool trailingComma = true) {
    json << "\"" << key << "\":" << (value ? "true" : "false");
    if (trailingComma) json << ",";
}

void AppendJsonInt(std::ostringstream& json, const char* key, std::size_t value, bool trailingComma = true) {
    json << "\"" << key << "\":" << value;
    if (trailingComma) json << ",";
}

} // namespace

void AdminBackend::SetHandlers(Handlers handlers) {
    m_handlers = std::move(handlers);
}

AdminBackend::HandleResult AdminBackend::HandleMessage(std::wstring_view payload) const {
    HandleResult result;
    const std::wstring kind = JsonStringField(payload, L"kind");
    if (kind == L"ready" || kind == L"request-snapshot") {
        result.requestSnapshot = true;
        return result;
    }
    if (kind != L"command") {
        result.logLine = L"Admin shell ignored message: " + std::wstring(payload);
        return result;
    }

    const std::wstring command = JsonStringField(payload, L"command");
    result.stateChanged = true;

    if (command == L"refresh-network") {
        if (m_handlers.refreshNetwork) m_handlers.refreshNetwork();
    } else if (command == L"generate-room-token") {
        if (m_handlers.generateRoomToken) m_handlers.generateRoomToken();
    } else if (command == L"apply-session") {
        if (m_handlers.applySessionConfig) {
            const std::wstring room = JsonStringField(payload, L"room");
            const std::wstring token = JsonStringField(payload, L"token");
            const std::wstring bind = JsonStringField(payload, L"bind");
            std::size_t port = 0;
            if (!JsonIntField(payload, L"port", port) || port == 0 || port > 65535) {
                port = 9443;
            }
            m_handlers.applySessionConfig(room, token, bind, static_cast<int>(port));
        }
    } else if (command == L"start-server") {
        if (m_handlers.startServer) m_handlers.startServer();
    } else if (command == L"stop-server") {
        if (m_handlers.stopServer) m_handlers.stopServer();
    } else if (command == L"service-only") {
        if (m_handlers.startServiceOnly) m_handlers.startServiceOnly();
    } else if (command == L"start-and-open-host") {
        if (m_handlers.startAndOpenHost) m_handlers.startAndOpenHost();
    } else if (command == L"open-viewer") {
        if (m_handlers.openViewer) m_handlers.openViewer();
    } else if (command == L"open-host") {
        if (m_handlers.openHost) m_handlers.openHost();
    } else if (command == L"copy-host-url") {
        if (m_handlers.copyHostUrl) m_handlers.copyHostUrl();
    } else if (command == L"copy-viewer-url") {
        if (m_handlers.copyViewerUrl) m_handlers.copyViewerUrl();
    } else if (command == L"export-bundle") {
        if (m_handlers.exportBundle) m_handlers.exportBundle();
    } else if (command == L"open-output") {
        if (m_handlers.openOutput) m_handlers.openOutput();
    } else if (command == L"open-report") {
        if (m_handlers.openReport) m_handlers.openReport();
    } else if (command == L"refresh-bundle") {
        if (m_handlers.refreshBundle) m_handlers.refreshBundle();
    } else if (command == L"show-share-wizard") {
        if (m_handlers.showShareWizard) m_handlers.showShareWizard();
    } else if (command == L"select-adapter") {
        if (m_handlers.selectNetworkCandidate) {
            std::size_t index = 0;
            if (JsonIntField(payload, L"index", index)) {
                m_handlers.selectNetworkCandidate(index);
            }
        }
    } else if (command == L"apply-hotspot") {
        if (m_handlers.applyHotspotConfig) {
            const std::wstring ssid = JsonStringField(payload, L"ssid");
            const std::wstring password = JsonStringField(payload, L"password");
            m_handlers.applyHotspotConfig(ssid, password);
        }
    } else if (command == L"start-hotspot") {
        if (m_handlers.startHotspot) m_handlers.startHotspot();
    } else if (command == L"stop-hotspot") {
        if (m_handlers.stopHotspot) m_handlers.stopHotspot();
    } else if (command == L"auto-hotspot") {
        if (m_handlers.autoHotspot) m_handlers.autoHotspot();
    } else if (command == L"open-hotspot-settings") {
        if (m_handlers.openHotspotSettings) m_handlers.openHotspotSettings();
    } else if (command == L"open-connected-devices") {
        if (m_handlers.openConnectedDevices) m_handlers.openConnectedDevices();
    } else if (command == L"switch-page") {
        if (m_handlers.navigatePage) {
            const std::wstring page = JsonStringField(payload, L"page");
            if (!page.empty()) {
                m_handlers.navigatePage(page);
            } else {
                result.stateChanged = false;
            }
        }
    } else {
        result.stateChanged = false;
        result.logLine = L"Admin shell unknown command: " + command;
    }

    return result;
}

std::wstring AdminBackend::BuildSnapshotEventJson(const Snapshot& snapshot) const {
    std::ostringstream json;
    json << "{";
    AppendJsonString(json, "type", L"event");
    AppendJsonString(json, "name", L"state.snapshot");
    json << "\"payload\":{";
    AppendJsonString(json, "app", snapshot.appName);
    AppendJsonString(json, "nativePage", snapshot.nativePage);
    AppendJsonString(json, "dashboardState", snapshot.dashboardState);
    AppendJsonString(json, "dashboardLabel", snapshot.dashboardLabel);
    AppendJsonString(json, "dashboardError", snapshot.dashboardError);
    AppendJsonBool(json, "canStartSharing", snapshot.canStartSharing);
    AppendJsonBool(json, "sharingActive", snapshot.sharingActive);
    AppendJsonBool(json, "serverRunning", snapshot.serverRunning);
    AppendJsonBool(json, "healthReady", snapshot.healthReady);
    AppendJsonBool(json, "hostReachable", snapshot.hostReachable);
    AppendJsonBool(json, "certReady", snapshot.certReady);
    AppendJsonString(json, "certDetail", snapshot.certDetail);
    AppendJsonString(json, "certExpectedSans", snapshot.certExpectedSans);
    AppendJsonBool(json, "wifiAdapterPresent", snapshot.wifiAdapterPresent);
    AppendJsonBool(json, "hotspotSupported", snapshot.hotspotSupported);
    AppendJsonBool(json, "hotspotRunning", snapshot.hotspotRunning);
    AppendJsonBool(json, "wifiDirectAvailable", snapshot.wifiDirectAvailable);
    AppendJsonInt(json, "activeIpv4Candidates", static_cast<std::size_t>(snapshot.activeIpv4Candidates));
    AppendJsonInt(json, "port", static_cast<std::size_t>(snapshot.port));
    AppendJsonInt(json, "rooms", snapshot.rooms);
    AppendJsonInt(json, "viewers", snapshot.viewers);
    AppendJsonString(json, "hostIp", snapshot.hostIp);
    AppendJsonString(json, "bind", snapshot.bind);
    AppendJsonString(json, "room", snapshot.room);
    AppendJsonString(json, "token", snapshot.token);
    AppendJsonString(json, "hostUrl", snapshot.hostUrl);
    AppendJsonString(json, "viewerUrl", snapshot.viewerUrl);
    AppendJsonString(json, "networkMode", snapshot.networkMode);
    AppendJsonString(json, "hostState", snapshot.hostState);
    AppendJsonString(json, "hotspotStatus", snapshot.hotspotStatus);
    AppendJsonString(json, "hotspotSsid", snapshot.hotspotSsid);
    AppendJsonString(json, "hotspotPassword", snapshot.hotspotPassword);
    AppendJsonString(json, "webviewStatus", snapshot.webviewStatus);
    AppendJsonString(json, "recentHeartbeat", snapshot.recentHeartbeat);
    AppendJsonString(json, "localReachability", snapshot.localReachability);
    AppendJsonString(json, "outputDir", snapshot.outputDir);
    AppendJsonString(json, "bundleDir", snapshot.bundleDir);
    AppendJsonString(json, "serverExePath", snapshot.serverExePath);
    AppendJsonString(json, "certDir", snapshot.certDir);
    AppendJsonString(json, "timelineText", snapshot.timelineText);
    AppendJsonString(json, "logTail", snapshot.logTail);
    AppendJsonBool(json, "viewerUrlCopied", snapshot.viewerUrlCopied);
    AppendJsonBool(json, "shareBundleExported", snapshot.shareBundleExported);
    AppendJsonString(json, "lastError", snapshot.lastError);
    AppendJsonInt(json, "defaultPort", static_cast<std::size_t>(snapshot.defaultPort));
    AppendJsonString(json, "defaultBind", snapshot.defaultBind);
    AppendJsonString(json, "roomRule", snapshot.roomRule);
    AppendJsonString(json, "tokenRule", snapshot.tokenRule);
    AppendJsonString(json, "logLevel", snapshot.logLevel);
    AppendJsonString(json, "defaultViewerOpenMode", snapshot.defaultViewerOpenMode);
    AppendJsonBool(json, "autoCopyViewerLink", snapshot.autoCopyViewerLink);
    AppendJsonBool(json, "autoGenerateQr", snapshot.autoGenerateQr);
    AppendJsonBool(json, "autoExportBundle", snapshot.autoExportBundle);
    AppendJsonBool(json, "saveStdStreams", snapshot.saveStdStreams);
    AppendJsonString(json, "certBypassPolicy", snapshot.certBypassPolicy);
    AppendJsonString(json, "webViewBehavior", snapshot.webViewBehavior);
    AppendJsonString(json, "startupHook", snapshot.startupHook);
    json << "\"networkCandidates\":[";
    for (std::size_t i = 0; i < snapshot.networkCandidates.size(); ++i) {
        const auto& candidate = snapshot.networkCandidates[i];
        json << "{";
        AppendJsonString(json, "name", candidate.name);
        AppendJsonString(json, "ip", candidate.ip);
        AppendJsonString(json, "type", candidate.type);
        AppendJsonBool(json, "recommended", candidate.recommended);
        AppendJsonBool(json, "selected", candidate.selected, false);
        json << "}";
        if (i + 1 < snapshot.networkCandidates.size()) {
            json << ",";
        }
    }
    json << "],";
    AppendJsonString(json, "lastErrorSummary", snapshot.lastError, false);
    json << "}}";
    return urlutil::Utf8ToWide(json.str());
}
