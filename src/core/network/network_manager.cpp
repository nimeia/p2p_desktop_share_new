#include "network_manager.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cwctype>
#include <initializer_list>
#include <limits>
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

namespace lan::network {

namespace {

#if defined(_WIN32)
std::string WideToUtf8(const wchar_t* value) {
  if (!value || !*value) return {};
  const int needed = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (needed <= 1) return {};
  std::string out(static_cast<std::size_t>(needed - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), needed, nullptr, nullptr);
  return out;
}

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
  while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
    value.pop_back();
  }
  std::size_t start = 0;
  while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\r' || value[start] == '\n')) {
    ++start;
  }
  return value.substr(start);
}

std::wstring Trim(std::wstring value) {
  while (!value.empty() && std::iswspace(value.back())) {
    value.pop_back();
  }
  std::size_t start = 0;
  while (start < value.size() && std::iswspace(value[start])) {
    ++start;
  }
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

std::string NormalizeSpaces(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  bool prevSpace = false;
  for (char c : text) {
    const bool isSpace = (c == ' ' || c == '\t' || c == '\r' || c == '\n');
    if (isSpace) {
      if (!prevSpace) out.push_back(' ');
      prevSpace = true;
    } else {
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
      prevSpace = false;
    }
  }
  return Trim(std::move(out));
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

bool IsPrivateIpv4(std::uint32_t hostOrderIp) {
  if ((hostOrderIp & 0xFF000000u) == 0x0A000000u) return true;
  if ((hostOrderIp & 0xFFF00000u) == 0xAC100000u) return true;
  if ((hostOrderIp & 0xFFFF0000u) == 0xC0A80000u) return true;
  return false;
}

bool IsApipa(std::uint32_t hostOrderIp) {
  return (hostOrderIp & 0xFFFF0000u) == 0xA9FE0000u;
}

bool HasGateway(const IP_ADAPTER_ADDRESSES* adapter) {
  return adapter && adapter->FirstGatewayAddress && adapter->FirstGatewayAddress->Address.lpSockaddr;
}

std::string GuessMode(const IP_ADAPTER_ADDRESSES* adapter, std::string_view text) {
  if (ContainsAny(text, {"wi-fi direct", "wifi direct", "miracast"})) return "wifi-direct";
  if (ContainsAny(text, {"mobile hotspot", "hotspot", "hosted"})) return "hotspot";
  if (adapter && adapter->IfType == IF_TYPE_IEEE80211) return "wifi";
  return "lan";
}

int ScoreAdapter(const IP_ADAPTER_ADDRESSES* adapter, std::uint32_t hostOrderIp, std::string_view text) {
  int score = 0;

  if (IsPrivateIpv4(hostOrderIp)) score += 200;
  else score += 20;

  if (adapter->IfType == IF_TYPE_IEEE80211) score += 40;
  if (HasGateway(adapter)) score += 20;
  if (adapter->Ipv4Enabled) score += 10;

  if (ContainsAny(text, {"wi-fi direct", "wifi direct", "miracast"})) score += 60;
  if (ContainsAny(text, {"mobile hotspot", "hotspot", "hosted"})) score += 50;

  if (ContainsAny(text, {"virtual", "hyper-v", "vmware", "vethernet", "virtualbox", "wsl"})) score -= 120;
  if (ContainsAny(text, {"bluetooth", "loopback", "teredo", "pseudo", "isatap", "vpn", "tap", "tun"})) score -= 80;
  if (IsApipa(hostOrderIp)) score -= 200;

  return score;
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

  const BOOL ok = CreateProcessW(
      nullptr,
      mutableCmd.data(),
      nullptr,
      nullptr,
      TRUE,
      CREATE_NO_WINDOW,
      nullptr,
      nullptr,
      &si,
      &pi);

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

  if (rawOutput.size() >= 2 &&
      static_cast<unsigned char>(rawOutput[0]) == 0xFF &&
      static_cast<unsigned char>(rawOutput[1]) == 0xFE) {
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
  const std::wstring cmd = L"cmd.exe /C netsh " + args;
  return RunProcessCapture(cmd, output, exitCode, err);
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
  if (ContainsAny(normalized, {
          L"status : started",
          L"status started",
          L"\x72B6\x6001 : \x5DF2\x542F\x52A8",
          L"\x72B6\x6001 \x5DF2\x542F\x52A8"})) {
    return true;
  }
  if (ContainsAny(normalized, {
          L"status : not started",
          L"status not started",
          L"\x72B6\x6001 : \x672A\x542F\x52A8",
          L"\x72B6\x6001 \x672A\x542F\x52A8"})) {
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

bool QueryWifiDirectApiAvailability() {
  HMODULE wlanApi = LoadLibraryW(L"wlanapi.dll");
  if (!wlanApi) return false;
  const bool available = GetProcAddress(wlanApi, "WFDOpenHandle") != nullptr;
  FreeLibrary(wlanApi);
  return available;
}

HotspotConfig MakeSuggestedHotspotConfigImpl() {
  static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  std::random_device rd;
  std::mt19937 rng(rd());
  std::uniform_int_distribution<int> dist(0, static_cast<int>(sizeof(kAlphabet) - 2));

  HotspotConfig cfg;
  cfg.ssid = "LanShare-";
  for (int i = 0; i < 6; ++i) cfg.ssid.push_back(kAlphabet[dist(rng)]);
  for (int i = 0; i < 12; ++i) cfg.password.push_back(kAlphabet[dist(rng)]);
  return cfg;
}

#endif

} // namespace

#if defined(_WIN32)

bool NetworkManager::GetCurrentNetworkInfo(NetworkInfo& out, std::string& err) {
  out = {};

  ULONG bufLen = 16 * 1024;
  std::vector<unsigned char> storage(bufLen);

  auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
  const ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS |
                      GAA_FLAG_SKIP_ANYCAST |
                      GAA_FLAG_SKIP_MULTICAST |
                      GAA_FLAG_SKIP_DNS_SERVER;

  ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen);
  if (ret == ERROR_BUFFER_OVERFLOW) {
    storage.resize(bufLen);
    addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    ret = GetAdaptersAddresses(AF_INET, flags, nullptr, addrs, &bufLen);
  }
  if (ret != NO_ERROR) {
    err = "GetAdaptersAddresses failed.";
    return false;
  }

  bool found = false;
  int bestScore = std::numeric_limits<int>::min();

  for (auto* adapter = addrs; adapter; adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp) continue;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL) continue;

    const std::string friendly = WideToUtf8(adapter->FriendlyName);
    const std::string desc = WideToUtf8(adapter->Description);
    const std::string combined = ToLower(friendly + " " + desc);

    for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
      const auto* sa = unicast->Address.lpSockaddr;
      if (!sa || sa->sa_family != AF_INET) continue;

      const auto* sin = reinterpret_cast<const sockaddr_in*>(sa);
      const std::uint32_t hostOrderIp = ntohl(sin->sin_addr.S_un.S_addr);
      if ((hostOrderIp & 0xFF000000u) == 0x7F000000u) continue;
      if (hostOrderIp == 0) continue;

      char ipStr[INET_ADDRSTRLEN]{};
      if (!InetNtopA(AF_INET, const_cast<IN_ADDR*>(&sin->sin_addr), ipStr, static_cast<DWORD>(std::size(ipStr)))) {
        continue;
      }

      const int score = ScoreAdapter(adapter, hostOrderIp, combined);
      if (!found || score > bestScore) {
        found = true;
        bestScore = score;
        out.hostIp = ipStr;
        out.mode = GuessMode(adapter, combined);
        out.ssid.clear();
        out.password.clear();
      }
    }
  }

  if (found) {
    err.clear();
    return true;
  }

  err = "No active IPv4 adapter was detected.";
  return false;
}

bool NetworkManager::QueryCapabilities(NetworkCapabilities& out, std::string& err) {
  out = {};
  out.wifiAdapterPresent = HasWifiAdapter();
  out.wifiDirectApiAvailable = QueryWifiDirectApiAvailability();
  out.wifiDirectNeedsPairingUi = out.wifiDirectApiAvailable;

  std::string output;
  DWORD exitCode = 0;
  if (RunNetshCapture(L"wlan show drivers", output, exitCode, err) && exitCode == 0) {
    if (const auto hosted = ParseHostedNetworkSupported(output); hosted.has_value()) {
      out.hotspotSupported = *hosted;
    } else {
      out.hotspotSupported = out.wifiAdapterPresent;
    }
    out.summary = Trim(output);
  } else {
    out.hotspotSupported = out.wifiAdapterPresent;
    if (err.empty()) err = "Failed to query WLAN driver info via netsh.";
  }

  if (out.summary.empty()) {
    out.summary = std::string("wifiAdapter=") + (out.wifiAdapterPresent ? "yes" : "no") +
                  ", hotspot=" + (out.hotspotSupported ? "yes" : "no") +
                  ", wifiDirectApi=" + (out.wifiDirectApiAvailable ? "yes" : "no");
  }
  return true;
}

HotspotConfig NetworkManager::MakeSuggestedHotspotConfig() {
  return MakeSuggestedHotspotConfigImpl();
}

bool NetworkManager::StartHotspot(const HotspotConfig& cfg, HotspotState& out, std::string& err) {
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

  std::string output;
  DWORD exitCode = 0;
  const std::wstring ssid = QuoteArg(Utf8ToWide(cfg.ssid));
  const std::wstring pwd = QuoteArg(Utf8ToWide(cfg.password));

  if (!RunNetshCapture(L"wlan set hostednetwork mode=allow ssid=" + ssid + L" key=" + pwd, output, exitCode, err)) {
    return false;
  }
  if (exitCode != 0) {
    err = "netsh set hostednetwork failed: " + Trim(output);
    return false;
  }

  output.clear();
  if (!RunNetshCapture(L"wlan start hostednetwork", output, exitCode, err)) {
    return false;
  }
  if (exitCode != 0) {
    err = "netsh start hostednetwork failed: " + Trim(output);
    return false;
  }

  out.supported = true;
  out.running = true;
  out.mode = "hotspot";
  out.ssid = cfg.ssid;
  out.password = cfg.password;
  out.rawStatus = Trim(output);

  NetworkInfo info;
  std::string infoErr;
  if (GetCurrentNetworkInfo(info, infoErr)) {
    out.hostIp = info.hostIp;
    if (!info.mode.empty()) out.mode = info.mode;
  }

  return true;
}

bool NetworkManager::StopHotspot(std::string& err) {
  std::string output;
  DWORD exitCode = 0;
  if (!RunNetshCapture(L"wlan stop hostednetwork", output, exitCode, err)) {
    return false;
  }
  if (exitCode != 0) {
    err = "netsh stop hostednetwork failed: " + Trim(output);
    return false;
  }
  return true;
}

bool NetworkManager::QueryHotspotState(HotspotState& out, std::string& err) {
  out = {};
  std::string output;
  DWORD exitCode = 0;
  if (!RunNetshCapture(L"wlan show hostednetwork", output, exitCode, err)) {
    return false;
  }
  if (exitCode != 0) {
    err = "netsh show hostednetwork failed: " + Trim(output);
    return false;
  }

  out.supported = true;
  out.rawStatus = Trim(output);
  if (const auto running = ParseHostedNetworkState(output); running.has_value()) {
    out.running = *running;
  }

  NetworkInfo info;
  std::string infoErr;
  if (GetCurrentNetworkInfo(info, infoErr)) {
    out.hostIp = info.hostIp;
    out.mode = info.mode;
  }

  return true;
}

#else

bool NetworkManager::GetCurrentNetworkInfo(NetworkInfo& out, std::string& err) {
  out = {};
  err = "NetworkManager::GetCurrentNetworkInfo is only implemented on Windows in this MVP.";
  return false;
}

bool NetworkManager::QueryCapabilities(NetworkCapabilities& out, std::string& err) {
  out = {};
  err = "NetworkManager::QueryCapabilities is only implemented on Windows in this MVP.";
  return false;
}

HotspotConfig NetworkManager::MakeSuggestedHotspotConfig() {
  return {"LanShare", "LanShare123"};
}

bool NetworkManager::StartHotspot(const HotspotConfig&, HotspotState& out, std::string& err) {
  out = {};
  err = "NetworkManager::StartHotspot is only implemented on Windows in this MVP.";
  return false;
}

bool NetworkManager::StopHotspot(std::string& err) {
  err = "NetworkManager::StopHotspot is only implemented on Windows in this MVP.";
  return false;
}

bool NetworkManager::QueryHotspotState(HotspotState& out, std::string& err) {
  out = {};
  err = "NetworkManager::QueryHotspotState is only implemented on Windows in this MVP.";
  return false;
}

#endif

} // namespace lan::network
