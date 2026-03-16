#include "platform/windows/network_actions_win.h"

#include "platform/windows/network_probe_win.h"
#include "core/network/endpoint_selection.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <initializer_list>
#include <optional>
#include <random>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

namespace lan::platform::windows {
#if defined(_WIN32)
namespace {

std::string WideToUtf8(std::wstring_view value) {
  if (value.empty()) return {};
  const int needed = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (needed <= 0) return {};
  std::string out(static_cast<std::size_t>(needed), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), needed, nullptr, nullptr);
  return out;
}

std::wstring Utf8ToWide(std::string_view value) {
  if (value.empty()) return {};
  const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (needed <= 0) return {};
  std::wstring out(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), needed);
  return out;
}

std::wstring MultiByteToWide(UINT codePage, std::string_view value) {
  if (value.empty()) return {};
  const int needed = MultiByteToWideChar(codePage, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (needed <= 0) return {};
  std::wstring out(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(codePage, 0, value.data(), static_cast<int>(value.size()), out.data(), needed);
  return out;
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string Trim(std::string value) {
  return lan::network::TrimNetworkText(std::move(value));
}

std::wstring Trim(std::wstring value) {
  while (!value.empty() && std::iswspace(value.back())) value.pop_back();
  std::size_t start = 0;
  while (start < value.size() && std::iswspace(value[start])) ++start;
  return value.substr(start);
}

bool ContainsAny(std::string_view text, std::initializer_list<std::string_view> needles) {
  for (const auto needle : needles) {
    if (text.find(needle) != std::string_view::npos) return true;
  }
  return false;
}

bool ContainsAny(std::wstring_view text, std::initializer_list<std::wstring_view> needles) {
  for (const auto needle : needles) {
    if (text.find(needle) != std::wstring_view::npos) return true;
  }
  return false;
}

std::wstring NormalizeSpaces(std::wstring_view text) {
  std::wstring out;
  out.reserve(text.size());
  bool prevSpace = false;
  for (wchar_t c : text) {
    const bool isSpace = std::iswspace(c) != 0;
    if (isSpace) {
      if (!prevSpace) out.push_back(L' ');
      prevSpace = true;
    } else {
      out.push_back(static_cast<wchar_t>(std::towlower(c)));
      prevSpace = false;
    }
  }
  return Trim(std::move(out));
}

std::wstring QuoteArg(const std::wstring& value) {
  bool needQuote = false;
  for (wchar_t ch : value) {
    if (ch == L' ' || ch == L'\t' || ch == L'"') {
      needQuote = true;
      break;
    }
  }
  if (!needQuote) return value;

  std::wstring out = L"\"";
  for (wchar_t ch : value) {
    if (ch == L'"') out += L"\\\"";
    else out.push_back(ch);
  }
  out += L"\"";
  return out;
}

bool RunProcessCapture(const std::wstring& commandLine,
                       std::string& output,
                       DWORD& exitCode,
                       std::string& err) {
  output.clear();
  exitCode = static_cast<DWORD>(-1);
  err.clear();

  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE outRead = nullptr;
  HANDLE outWrite = nullptr;
  if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
    err = "CreatePipe failed.";
    return false;
  }
  SetHandleInformation(outRead, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = outWrite;
  si.hStdError = outWrite;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION pi{};
  std::vector<wchar_t> mutableCmd(commandLine.begin(), commandLine.end());
  mutableCmd.push_back(L'\0');

  const BOOL ok = CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  CloseHandle(outWrite);
  outWrite = nullptr;
  if (!ok) {
    CloseHandle(outRead);
    err = "CreateProcess failed.";
    return false;
  }

  std::string rawOutput;
  char buffer[1024];
  DWORD bytesRead = 0;
  while (ReadFile(outRead, buffer, static_cast<DWORD>(sizeof(buffer)), &bytesRead, nullptr) && bytesRead > 0) {
    rawOutput.append(buffer, buffer + bytesRead);
  }

  WaitForSingleObject(pi.hProcess, 20000);
  GetExitCodeProcess(pi.hProcess, &exitCode);
  CloseHandle(outRead);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  if (rawOutput.size() >= 2 && static_cast<unsigned char>(rawOutput[0]) == 0xFF && static_cast<unsigned char>(rawOutput[1]) == 0xFE) {
    const wchar_t* wideData = reinterpret_cast<const wchar_t*>(rawOutput.data() + 2);
    const std::size_t wideSize = (rawOutput.size() - 2) / sizeof(wchar_t);
    output = WideToUtf8(std::wstring_view(wideData, wideSize));
  } else {
    const std::wstring wideOutput = MultiByteToWide(CP_OEMCP, rawOutput);
    output = wideOutput.empty() ? rawOutput : WideToUtf8(wideOutput);
  }
  return true;
}

bool RunNetshCapture(const std::wstring& args, std::string& output, DWORD& exitCode, std::string& err) {
  return RunProcessCapture(L"cmd.exe /C netsh " + args, output, exitCode, err);
}

std::optional<bool> ParseHostedNetworkSupported(std::string_view text) {
  const std::wstring normalized = NormalizeSpaces(Utf8ToWide(text));
  std::size_t pos = normalized.find(L"hosted network supported");
  if (pos != std::wstring::npos) {
    const std::wstring tail = normalized.substr(pos, std::min<std::size_t>(80, normalized.size() - pos));
    if (ContainsAny(tail, {L": yes", L" yes"})) return true;
    if (ContainsAny(tail, {L": no", L" no"})) return false;
  }
  pos = normalized.find(L"\x627F\x8F7D\x7F51\x7EDC\x652F\x6301");
  if (pos != std::wstring::npos) {
    const std::wstring tail = normalized.substr(pos, std::min<std::size_t>(32, normalized.size() - pos));
    if (ContainsAny(tail, {L": \x662F", L" \x662F"})) return true;
    if (ContainsAny(tail, {L": \x5426", L" \x5426"})) return false;
  }
  pos = normalized.find(L"\x6258\x7BA1\x7F51\x7EDC\x652F\x6301");
  if (pos != std::wstring::npos) {
    const std::wstring tail = normalized.substr(pos, std::min<std::size_t>(32, normalized.size() - pos));
    if (ContainsAny(tail, {L": \x662F", L" \x662F"})) return true;
    if (ContainsAny(tail, {L": \x5426", L" \x5426"})) return false;
  }
  return std::nullopt;
}

std::optional<bool> ParseHostedNetworkState(std::string_view text) {
  const std::wstring normalized = NormalizeSpaces(Utf8ToWide(text));
  if (ContainsAny(normalized, {L"status : started", L"status started", L"\x72B6\x6001 : \x5DF2\x542F\x52A8", L"\x72B6\x6001 \x5DF2\x542F\x52A8"})) {
    return true;
  }
  if (ContainsAny(normalized, {L"status : not started", L"status not started", L"\x72B6\x6001 : \x672A\x542F\x52A8", L"\x72B6\x6001 \x672A\x542F\x52A8"})) {
    return false;
  }
  return std::nullopt;
}

bool HasWifiAdapter() {
  ULONG bufLen = 16 * 1024;
  std::vector<unsigned char> storage(bufLen);
  auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
  ULONG ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, addrs, &bufLen);
  if (ret == ERROR_BUFFER_OVERFLOW) {
    storage.resize(bufLen);
    addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES, nullptr, addrs, &bufLen);
  }
  if (ret != NO_ERROR) return false;
  for (auto* adapter = addrs; adapter; adapter = adapter->Next) {
    if (adapter->IfType == IF_TYPE_IEEE80211) return true;
  }
  return false;
}

bool IsProcessElevated() {
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
  TOKEN_ELEVATION elevation{};
  DWORD bytes = 0;
  const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytes);
  CloseHandle(token);
  return ok && elevation.TokenIsElevated != 0;
}

std::string FriendlyHostedNetworkUnavailableError(bool wifiAdapterPresent) {
  return wifiAdapterPresent
      ? "The current Wi-Fi driver does not support hosted network control. Use Windows Mobile Hotspot settings instead."
      : "No Wi-Fi adapter was detected. Use wired LAN or open Windows hotspot settings instead.";
}

std::string FriendlyHotspotAdminError() {
  return "Starting hotspot requires administrator privileges. Restart the desktop app as administrator, or use Windows Mobile Hotspot settings instead.";
}

bool QueryWifiDirectApiAvailability() {
  HMODULE wlanApi = LoadLibraryW(L"wlanapi.dll");
  if (!wlanApi) return false;
  const bool available = GetProcAddress(wlanApi, "WFDOpenHandle") != nullptr;
  FreeLibrary(wlanApi);
  return available;
}

} // namespace
#endif

bool QueryCapabilities(lan::network::NetworkCapabilities& out, std::string& err) {
#if defined(_WIN32)
  out = {};
  out.wifiAdapterPresent = HasWifiAdapter();
  out.processElevated = IsProcessElevated();
  out.wifiDirectApiAvailable = QueryWifiDirectApiAvailability();
  out.wifiDirectNeedsPairingUi = out.wifiDirectApiAvailable;

  std::string output;
  DWORD exitCode = 0;
  if (RunNetshCapture(L"wlan show drivers", output, exitCode, err) && exitCode == 0) {
    if (const auto hosted = ParseHostedNetworkSupported(output); hosted.has_value()) out.hotspotSupported = *hosted;
    else out.hotspotSupported = out.wifiAdapterPresent;
    out.summary = Trim(output);
  } else {
    out.hotspotSupported = out.wifiAdapterPresent;
    if (err.empty()) err = "Failed to query WLAN driver info via netsh.";
  }

  if (out.summary.empty()) {
    out.summary = std::string("wifiAdapter=") + (out.wifiAdapterPresent ? "yes" : "no") +
                  ", elevated=" + (out.processElevated ? "yes" : "no") +
                  ", hotspot=" + (out.hotspotSupported ? "yes" : "no") +
                  ", wifiDirectApi=" + (out.wifiDirectApiAvailable ? "yes" : "no");
  }
  return true;
#else
  out = {};
  err = "Windows network actions are unavailable on this platform.";
  return false;
#endif
}

lan::network::HotspotConfig MakeSuggestedHotspotConfig() {
  static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(kAlphabet) - 2));

  lan::network::HotspotConfig cfg;
  cfg.ssid = "LanShare-";
  for (int i = 0; i < 6; ++i) cfg.ssid.push_back(kAlphabet[dist(rng)]);
  for (int i = 0; i < 12; ++i) cfg.password.push_back(kAlphabet[dist(rng)]);
  return cfg;
}

bool StartHotspot(const lan::network::HotspotConfig& cfg, lan::network::HotspotState& out, std::string& err) {
#if defined(_WIN32)
  out = {};
  err.clear();
  if (cfg.ssid.empty()) {
    err = "Hotspot SSID is empty.";
    return false;
  }
  if (cfg.password.size() < 8) {
    err = "Hotspot password must be at least 8 characters.";
    return false;
  }

  lan::network::NetworkCapabilities caps;
  std::string capsErr;
  QueryCapabilities(caps, capsErr);
  out.supported = caps.hotspotSupported;
  if (!caps.hotspotSupported) {
    err = FriendlyHostedNetworkUnavailableError(caps.wifiAdapterPresent);
    return false;
  }

  std::string output;
  DWORD exitCode = 0;
  const std::wstring ssid = QuoteArg(Utf8ToWide(cfg.ssid));
  const std::wstring pwd = QuoteArg(Utf8ToWide(cfg.password));
  if (!RunNetshCapture(L"wlan set hostednetwork mode=allow ssid=" + ssid + L" key=" + pwd, output, exitCode, err)) return false;
  if (exitCode != 0) {
    const std::string trimmed = Trim(output);
    err = ToLower(trimmed).find("administrator privilege") != std::string::npos ? FriendlyHotspotAdminError() : "netsh set hostednetwork failed: " + trimmed;
    return false;
  }

  output.clear();
  if (!RunNetshCapture(L"wlan start hostednetwork", output, exitCode, err)) return false;
  if (exitCode != 0) {
    const std::string trimmed = Trim(output);
    err = ToLower(trimmed).find("administrator privilege") != std::string::npos ? FriendlyHotspotAdminError() : "netsh start hostednetwork failed: " + trimmed;
    return false;
  }

  out.supported = true;
  out.running = true;
  out.mode = "hotspot";
  out.ssid = cfg.ssid;
  out.password = cfg.password;
  out.rawStatus = Trim(output);

  lan::network::EndpointProbeResult probe;
  std::string probeErr;
  if (ProbeNetworkInterfaces(probe, probeErr)) {
    lan::network::NetworkInfo info;
    std::string detail;
    if (lan::network::SelectPreferredNetworkInfo(probe, info, detail)) {
      out.hostIp = info.hostIp;
      if (!info.mode.empty()) out.mode = info.mode;
    }
  }
  return true;
#else
  out = {};
  err = "Windows hotspot control is unavailable on this platform.";
  return false;
#endif
}

bool StopHotspot(std::string& err) {
#if defined(_WIN32)
  std::string output;
  DWORD exitCode = 0;
  if (!RunNetshCapture(L"wlan stop hostednetwork", output, exitCode, err)) return false;
  if (exitCode != 0) {
    err = "netsh stop hostednetwork failed: " + Trim(output);
    return false;
  }
  return true;
#else
  err = "Windows hotspot control is unavailable on this platform.";
  return false;
#endif
}

bool QueryHotspotState(lan::network::HotspotState& out, std::string& err) {
#if defined(_WIN32)
  out = {};
  lan::network::NetworkCapabilities caps;
  std::string capsErr;
  QueryCapabilities(caps, capsErr);
  out.supported = caps.hotspotSupported;

  std::string output;
  DWORD exitCode = 0;
  if (!RunNetshCapture(L"wlan show hostednetwork", output, exitCode, err)) return false;
  if (exitCode != 0) {
    err = "netsh show hostednetwork failed: " + Trim(output);
    return false;
  }
  out.rawStatus = Trim(output);
  if (const auto running = ParseHostedNetworkState(output); running.has_value()) out.running = *running;

  lan::network::EndpointProbeResult probe;
  std::string probeErr;
  if (ProbeNetworkInterfaces(probe, probeErr)) {
    lan::network::NetworkInfo info;
    std::string detail;
    if (lan::network::SelectPreferredNetworkInfo(probe, info, detail)) {
      out.hostIp = info.hostIp;
      out.mode = info.mode;
    }
  }
  return true;
#else
  out = {};
  err = "Windows hotspot control is unavailable on this platform.";
  return false;
#endif
}

} // namespace lan::platform::windows
