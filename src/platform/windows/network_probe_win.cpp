#include "platform/windows/network_probe_win.h"

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace lan::platform::windows {
#if defined(_WIN32)
namespace {

std::string WideToUtf8(const wchar_t* value) {
  if (!value || !*value) return {};
  const int needed = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
  if (needed <= 1) return {};
  std::string out(static_cast<std::size_t>(needed - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value, -1, out.data(), needed, nullptr, nullptr);
  return out;
}

std::string GuessMode(const IP_ADAPTER_ADDRESSES* adapter, std::string combined) {
  std::transform(combined.begin(), combined.end(), combined.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (combined.find("wi-fi direct") != std::string::npos || combined.find("wifi direct") != std::string::npos ||
      combined.find("miracast") != std::string::npos) {
    return "wifi-direct";
  }
  if (combined.find("mobile hotspot") != std::string::npos || combined.find("hotspot") != std::string::npos ||
      combined.find("hosted") != std::string::npos) {
    return "hotspot";
  }
  if (adapter && adapter->IfType == IF_TYPE_IEEE80211) return "wifi";
  return "lan";
}

} // namespace
#endif

bool ProbeNetworkInterfaces(lan::network::EndpointProbeResult& out, std::string& err) {
  out = {};
  err.clear();

#if defined(_WIN32)
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
    out.detail = err;
    return false;
  }

  for (auto* adapter = addrs; adapter; adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp) continue;
    if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK || adapter->IfType == IF_TYPE_TUNNEL) continue;

    const std::string friendly = WideToUtf8(adapter->FriendlyName);
    const std::string desc = WideToUtf8(adapter->Description);
    const std::string combined = friendly + " " + desc;

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

      lan::network::EndpointProbeCandidate candidate;
      candidate.interfaceName = friendly;
      candidate.interfaceDescription = desc;
      candidate.hostIp = ipStr;
      candidate.mode = GuessMode(adapter, combined);
      candidate.isUp = adapter->OperStatus == IfOperStatusUp;
      candidate.isRunning = adapter->OperStatus == IfOperStatusUp;
      candidate.isLoopback = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
      candidate.isTunnel = adapter->IfType == IF_TYPE_TUNNEL;
      candidate.isVirtual = false;
      candidate.isPrivateIpv4 = lan::network::LooksLikePrivateIpv4(candidate.hostIp);
      candidate.isApipa = lan::network::LooksLikeApipa(candidate.hostIp);
      candidate.hasGateway = adapter->FirstGatewayAddress && adapter->FirstGatewayAddress->Address.lpSockaddr;
      candidate.isWifi = adapter->IfType == IF_TYPE_IEEE80211;
      candidate.ipv4Enabled = adapter->Ipv4Enabled != 0;
      out.candidates.push_back(std::move(candidate));
    }
  }

  out.detail = out.candidates.empty()
      ? "No active IPv4 adapter was detected."
      : "Collected IPv4 adapters using GetAdaptersAddresses().";
  return !out.candidates.empty();
#else
  err = "Windows network probing is unavailable on this platform.";
  out.detail = err;
  return false;
#endif
}

} // namespace lan::platform::windows
