#include "platform/posix/network_probe_posix.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>

namespace lan::platform::posix {

bool ProbeNetworkInterfaces(lan::network::EndpointProbeResult& out, std::string& err) {
  out = {};
  err.clear();

  ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) {
    err = std::string("getifaddrs failed: ") + std::strerror(errno);
    out.detail = err;
    return false;
  }

  for (const ifaddrs* entry = list; entry; entry = entry->ifa_next) {
    if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET) continue;

    const auto* addr = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
    char buffer[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer))) continue;

    lan::network::EndpointProbeCandidate candidate;
    candidate.interfaceName = entry->ifa_name ? entry->ifa_name : "unknown";
    candidate.interfaceDescription = candidate.interfaceName;
    candidate.hostIp = buffer;
    candidate.isUp = (entry->ifa_flags & IFF_UP) != 0;
    candidate.isRunning = (entry->ifa_flags & IFF_RUNNING) != 0;
    candidate.isLoopback = (entry->ifa_flags & IFF_LOOPBACK) != 0;
    candidate.isTunnel = false;
    candidate.isVirtual = false;
    candidate.isPrivateIpv4 = lan::network::LooksLikePrivateIpv4(candidate.hostIp);
    candidate.isApipa = lan::network::LooksLikeApipa(candidate.hostIp);
    candidate.hasGateway = false;
    candidate.isWifi = false;
    candidate.ipv4Enabled = true;
    candidate.mode = "lan";
    out.candidates.push_back(std::move(candidate));
  }

  freeifaddrs(list);

  out.detail = out.candidates.empty()
      ? "No non-loopback IPv4 interface is currently available."
      : "Collected IPv4 interfaces using getifaddrs().";
  return !out.candidates.empty();
}

} // namespace lan::platform::posix
