#pragma once

#include "core/network/network_types.h"

#include <string>
#include <string_view>
#include <vector>

namespace lan::network {

inline constexpr const char* kAutoAddressToken = "auto";

struct EndpointProbeCandidate {
  std::string interfaceName;
  std::string interfaceDescription;
  std::string mode = "lan";
  std::string hostIp;
  bool isUp = false;
  bool isRunning = false;
  bool isLoopback = false;
  bool isTunnel = false;
  bool isVirtual = false;
  bool isPrivateIpv4 = false;
  bool isApipa = false;
  bool hasGateway = false;
  bool isWifi = false;
  bool ipv4Enabled = true;
  int scoreAdjustment = 0;
};

struct EndpointProbeResult {
  std::vector<EndpointProbeCandidate> candidates;
  std::string detail;
};

struct EndpointSelectionRequest {
  std::string bindAddress = "0.0.0.0";
  std::string subjectAltNames = kAutoAddressToken;
};

struct EndpointSelection {
  std::string bindAddress = "0.0.0.0";
  std::string subjectAltNames = "127.0.0.1";
  std::string preferredHost = "127.0.0.1";
  bool usedAutoDiscovery = false;
  bool networkInfoAvailable = false;
  std::string detail;
  NetworkInfo networkInfo;
};

std::string TrimNetworkText(std::string value);
bool IsAutoEndpointValue(std::string_view value);
std::vector<std::string> SplitEndpointEntries(std::string_view value);
std::string FirstEndpointEntryOr(std::string_view value, std::string_view fallback);

bool LooksLikePrivateIpv4(std::string_view value);
bool LooksLikeApipa(std::string_view value);
int ScoreEndpointProbeCandidate(const EndpointProbeCandidate& candidate);
bool SelectPreferredNetworkInfo(const EndpointProbeResult& probe, NetworkInfo& out, std::string& detail);
EndpointSelection ResolveEndpointSelection(const EndpointSelectionRequest& request,
                                           const EndpointProbeResult* probe);

} // namespace lan::network
