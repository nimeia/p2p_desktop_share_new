#include "core/runtime/shell_bridge_presenter.h"

#include <algorithm>
#include <cstdlib>
#include <cwctype>
#include <codecvt>
#include <cstdio>
#include <locale>
#include <sstream>

namespace lan::runtime {
namespace {

std::wstring JsonStringField(std::wstring_view body, std::wstring_view key) {
  const std::wstring pattern = L"\"" + std::wstring(key) + L"\"";
  std::size_t pos = body.find(pattern);
  if (pos == std::wstring::npos) return L"";
  pos = body.find(L':', pos + pattern.size());
  if (pos == std::wstring::npos) return L"";
  ++pos;
  while (pos < body.size() && iswspace(body[pos])) ++pos;
  if (pos >= body.size() || body[pos] != L'\"') return L"";
  ++pos;

  std::wstring value;
  while (pos < body.size()) {
    const wchar_t ch = body[pos++];
    if (ch == L'\\' && pos < body.size()) {
      const wchar_t next = body[pos++];
      switch (next) {
        case L'\"': value.push_back(L'\"'); break;
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
    if (ch == L'\"') {
      return value;
    }
    value.push_back(ch);
  }
  return L"";
}

bool JsonHasField(std::wstring_view body, std::wstring_view key) {
  const std::wstring pattern = L"\"" + std::wstring(key) + L"\"";
  const std::size_t pos = body.find(pattern);
  if (pos == std::wstring::npos) return false;
  return body.find(L':', pos + pattern.size()) != std::wstring::npos;
}

bool JsonIntField(std::wstring_view body, std::wstring_view key, std::size_t& value) {
  const std::wstring pattern = L"\"" + std::wstring(key) + L"\"";
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
  value = static_cast<std::size_t>(std::wcstoul(std::wstring(body.substr(pos, end - pos)).c_str(), nullptr, 10));
  return true;
}

std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> MakeWideConverter() {
  return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>();
}

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  auto converter = MakeWideConverter();
  return converter.to_bytes(value.data(), value.data() + value.size());
}

std::string JsonEscapeUtf8(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 16);
  for (const unsigned char c : value) {
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
  json << '"' << key << "\":\"" << JsonEscapeUtf8(WideToUtf8(value)) << '"';
  if (trailingComma) json << ',';
}

void AppendJsonBool(std::ostringstream& json, const char* key, bool value, bool trailingComma = true) {
  json << '"' << key << "\":" << (value ? "true" : "false");
  if (trailingComma) json << ',';
}

void AppendJsonInt(std::ostringstream& json, const char* key, std::size_t value, bool trailingComma = true) {
  json << '"' << key << "\":" << value;
  if (trailingComma) json << ',';
}

ShellBridgeAdminCommandKind ParseAdminCommandKind(std::wstring_view command) {
  if (command == L"refresh-network") return ShellBridgeAdminCommandKind::RefreshNetwork;
  if (command == L"generate-room-token") return ShellBridgeAdminCommandKind::GenerateRoomToken;
  if (command == L"apply-session") return ShellBridgeAdminCommandKind::ApplySession;
  if (command == L"start-server") return ShellBridgeAdminCommandKind::StartServer;
  if (command == L"stop-server") return ShellBridgeAdminCommandKind::StopServer;
  if (command == L"service-only") return ShellBridgeAdminCommandKind::ServiceOnly;
  if (command == L"start-and-open-host") return ShellBridgeAdminCommandKind::StartAndOpenHost;
  if (command == L"open-host") return ShellBridgeAdminCommandKind::OpenHost;
  if (command == L"open-viewer") return ShellBridgeAdminCommandKind::OpenViewer;
  if (command == L"copy-host-url") return ShellBridgeAdminCommandKind::CopyHostUrl;
  if (command == L"copy-viewer-url") return ShellBridgeAdminCommandKind::CopyViewerUrl;
  if (command == L"export-bundle") return ShellBridgeAdminCommandKind::ExportBundle;
  if (command == L"open-output") return ShellBridgeAdminCommandKind::OpenOutput;
  if (command == L"open-report") return ShellBridgeAdminCommandKind::OpenReport;
  if (command == L"refresh-bundle") return ShellBridgeAdminCommandKind::RefreshBundle;
  if (command == L"show-share-wizard") return ShellBridgeAdminCommandKind::ShowShareWizard;
  if (command == L"show-qr") return ShellBridgeAdminCommandKind::ShowQr;
  if (command == L"quick-fix-network") return ShellBridgeAdminCommandKind::QuickFixNetwork;
  if (command == L"quick-fix-sharing") return ShellBridgeAdminCommandKind::QuickFixSharing;
  if (command == L"quick-fix-handoff") return ShellBridgeAdminCommandKind::QuickFixHandoff;
  if (command == L"quick-fix-hotspot") return ShellBridgeAdminCommandKind::QuickFixHotspot;
  if (command == L"select-adapter") return ShellBridgeAdminCommandKind::SelectAdapter;
  if (command == L"apply-hotspot") return ShellBridgeAdminCommandKind::ApplyHotspot;
  if (command == L"start-hotspot") return ShellBridgeAdminCommandKind::StartHotspot;
  if (command == L"stop-hotspot") return ShellBridgeAdminCommandKind::StopHotspot;
  if (command == L"auto-hotspot") return ShellBridgeAdminCommandKind::AutoHotspot;
  if (command == L"open-hotspot-settings") return ShellBridgeAdminCommandKind::OpenHotspotSettings;
  if (command == L"open-firewall-settings") return ShellBridgeAdminCommandKind::OpenFirewallSettings;
  if (command == L"run-network-diagnostics") return ShellBridgeAdminCommandKind::RunNetworkDiagnostics;
  if (command == L"check-webview-runtime") return ShellBridgeAdminCommandKind::CheckWebViewRuntime;
  if (command == L"export-remote-probe-guide") return ShellBridgeAdminCommandKind::ExportRemoteProbeGuide;
  if (command == L"open-connected-devices") return ShellBridgeAdminCommandKind::OpenConnectedDevices;
  if (command == L"switch-page") return ShellBridgeAdminCommandKind::SwitchPage;
  if (command == L"set-language") return ShellBridgeAdminCommandKind::SetLanguage;
  return ShellBridgeAdminCommandKind::None;
}

ShellBridgeInboundMessage ParseAdminShellMessage(std::wstring_view payload, std::wstring_view kind) {
  ShellBridgeInboundMessage message;
  message.source = ShellBridgeSource::AdminShell;
  message.rawPayload = std::wstring(payload);

  if (kind == L"ready") {
    message.kind = ShellBridgeInboundKind::AdminReady;
    return message;
  }
  if (kind == L"request-snapshot") {
    message.kind = ShellBridgeInboundKind::AdminRequestSnapshot;
    return message;
  }
  if (kind != L"command") {
    message.kind = ShellBridgeInboundKind::Unknown;
    return message;
  }

  message.kind = ShellBridgeInboundKind::AdminCommand;
  message.adminCommand.commandName = JsonStringField(payload, L"command");
  message.adminCommand.kind = ParseAdminCommandKind(message.adminCommand.commandName);

  if (message.adminCommand.kind == ShellBridgeAdminCommandKind::ApplySession) {
    message.adminCommand.room = JsonStringField(payload, L"room");
    message.adminCommand.token = JsonStringField(payload, L"token");
    message.adminCommand.bind = JsonStringField(payload, L"bind");
    std::size_t port = 0;
    if (JsonIntField(payload, L"port", port) && port > 0 && port <= 65535) {
      message.adminCommand.port = static_cast<int>(port);
    }
  } else if (message.adminCommand.kind == ShellBridgeAdminCommandKind::SelectAdapter) {
    std::size_t index = 0;
    if (JsonIntField(payload, L"index", index)) {
      message.adminCommand.index = index;
      message.adminCommand.hasIndex = true;
    }
  } else if (message.adminCommand.kind == ShellBridgeAdminCommandKind::ApplyHotspot) {
    message.adminCommand.ssid = JsonStringField(payload, L"ssid");
    message.adminCommand.password = JsonStringField(payload, L"password");
  } else if (message.adminCommand.kind == ShellBridgeAdminCommandKind::SwitchPage) {
    message.adminCommand.page = JsonStringField(payload, L"page");
  } else if (message.adminCommand.kind == ShellBridgeAdminCommandKind::SetLanguage) {
    message.adminCommand.locale = JsonStringField(payload, L"locale");
  }

  return message;
}

} // namespace

ShellBridgeInboundMessage ParseShellBridgeInboundMessage(std::wstring_view payload) {
  const std::wstring source = JsonStringField(payload, L"source");
  const std::wstring kind = JsonStringField(payload, L"kind");

  if (source == L"admin-shell") {
    return ParseAdminShellMessage(payload, kind);
  }

  ShellBridgeInboundMessage message;
  message.rawPayload = std::wstring(payload);
  message.source = ShellBridgeSource::HostPage;

  if (kind == L"status") {
    message.kind = ShellBridgeInboundKind::HostStatus;
    message.hostState = JsonStringField(payload, L"state");
    if (JsonHasField(payload, L"captureState")) {
      message.hasCaptureState = true;
      message.captureState = JsonStringField(payload, L"captureState");
    }
    if (JsonHasField(payload, L"captureLabel")) {
      message.hasCaptureLabel = true;
      message.captureLabel = JsonStringField(payload, L"captureLabel");
    }
    std::size_t viewers = 0;
    if (JsonIntField(payload, L"viewers", viewers)) {
      message.viewers = viewers;
      message.hasViewers = true;
    }
    return message;
  }
  if (kind == L"log") {
    message.kind = ShellBridgeInboundKind::HostLog;
    message.logMessage = JsonStringField(payload, L"message");
    return message;
  }
  if (!kind.empty()) {
    message.kind = ShellBridgeInboundKind::Unknown;
    return message;
  }

  message.source = ShellBridgeSource::Unknown;
  message.kind = ShellBridgeInboundKind::None;
  return message;
}

std::wstring BuildShellBridgeSnapshotEventJson(const ShellBridgeSnapshotState& snapshot) {
  std::ostringstream json;
  json << "{";
  AppendJsonString(json, "type", L"event");
  AppendJsonString(json, "name", L"state.snapshot");
  json << "\"payload\":{";
  AppendJsonString(json, "locale", snapshot.localeCode);
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
  AppendJsonBool(json, "firewallReady", snapshot.firewallReady);
  AppendJsonString(json, "firewallDetail", snapshot.firewallDetail);
  AppendJsonBool(json, "remoteViewerReady", snapshot.remoteViewerReady);
  AppendJsonString(json, "remoteViewerDetail", snapshot.remoteViewerDetail);
  AppendJsonString(json, "remoteProbeLabel", snapshot.remoteProbeLabel);
  AppendJsonString(json, "remoteProbeAction", snapshot.remoteProbeAction);
  AppendJsonBool(json, "wifiAdapterPresent", snapshot.wifiAdapterPresent);
  AppendJsonBool(json, "hotspotSupported", snapshot.hotspotSupported);
  AppendJsonBool(json, "hotspotRunning", snapshot.hotspotRunning);
  AppendJsonBool(json, "wifiDirectAvailable", snapshot.wifiDirectAvailable);
  AppendJsonInt(json, "activeIpv4Candidates", static_cast<std::size_t>(std::max(snapshot.activeIpv4Candidates, 0)));
  AppendJsonInt(json, "port", static_cast<std::size_t>(std::max(snapshot.port, 0)));
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
  AppendJsonString(json, "captureState", snapshot.captureState);
  AppendJsonString(json, "captureLabel", snapshot.captureLabel);
  AppendJsonString(json, "hotspotStatus", snapshot.hotspotStatus);
  AppendJsonString(json, "hotspotSsid", snapshot.hotspotSsid);
  AppendJsonString(json, "hotspotPassword", snapshot.hotspotPassword);
  AppendJsonString(json, "webviewStatus", snapshot.webviewStatus);
  AppendJsonString(json, "recentHeartbeat", snapshot.recentHeartbeat);
  AppendJsonString(json, "localReachability", snapshot.localReachability);
  AppendJsonString(json, "outputDir", snapshot.outputDir);
  AppendJsonString(json, "bundleDir", snapshot.bundleDir);
  AppendJsonString(json, "serverExePath", snapshot.serverExePath);
  AppendJsonString(json, "adminDir", snapshot.adminDir);
  AppendJsonString(json, "timelineText", snapshot.timelineText);
  AppendJsonString(json, "logTail", snapshot.logTail);
  AppendJsonBool(json, "viewerUrlCopied", snapshot.viewerUrlCopied);
  AppendJsonBool(json, "shareBundleExported", snapshot.shareBundleExported);
  AppendJsonBool(json, "shareWizardOpened", snapshot.shareWizardOpened);
  AppendJsonBool(json, "handoffStarted", snapshot.handoffStarted);
  AppendJsonBool(json, "handoffDelivered", snapshot.handoffDelivered);
  AppendJsonString(json, "handoffState", snapshot.handoffState);
  AppendJsonString(json, "handoffLabel", snapshot.handoffLabel);
  AppendJsonString(json, "handoffDetail", snapshot.handoffDetail);
  AppendJsonString(json, "lastError", snapshot.lastError);
  AppendJsonInt(json, "defaultPort", static_cast<std::size_t>(std::max(snapshot.defaultPort, 0)));
  AppendJsonString(json, "defaultBind", snapshot.defaultBind);
  AppendJsonString(json, "roomRule", snapshot.roomRule);
  AppendJsonString(json, "tokenRule", snapshot.tokenRule);
  AppendJsonString(json, "logLevel", snapshot.logLevel);
  AppendJsonString(json, "defaultViewerOpenMode", snapshot.defaultViewerOpenMode);
  AppendJsonBool(json, "autoCopyViewerLink", snapshot.autoCopyViewerLink);
  AppendJsonBool(json, "autoGenerateQr", snapshot.autoGenerateQr);
  AppendJsonBool(json, "autoExportBundle", snapshot.autoExportBundle);
  AppendJsonBool(json, "saveStdStreams", snapshot.saveStdStreams);
  AppendJsonString(json, "webViewBehavior", snapshot.webViewBehavior);
  AppendJsonString(json, "startupHook", snapshot.startupHook);
  json << "\"networkCandidates\":[";
  for (std::size_t i = 0; i < snapshot.networkCandidates.size(); ++i) {
    const auto& candidate = snapshot.networkCandidates[i];
    json << '{';
    AppendJsonString(json, "name", candidate.name);
    AppendJsonString(json, "ip", candidate.ip);
    AppendJsonString(json, "type", candidate.type);
    AppendJsonBool(json, "recommended", candidate.recommended);
    AppendJsonBool(json, "selected", candidate.selected);
    AppendJsonBool(json, "probeReady", candidate.probeReady);
    AppendJsonString(json, "probeLabel", candidate.probeLabel);
    AppendJsonString(json, "probeDetail", candidate.probeDetail, false);
    json << '}';
    if (i + 1 < snapshot.networkCandidates.size()) {
      json << ',';
    }
  }
  json << "],";
  AppendJsonString(json, "lastErrorSummary", snapshot.lastError, false);
  json << "}}";

  auto converter = MakeWideConverter();
  return converter.from_bytes(json.str());
}

} // namespace lan::runtime
