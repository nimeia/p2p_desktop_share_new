#include "core/network/endpoint_selection.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <sstream>

namespace lan::network {
namespace {

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::optional<std::array<unsigned int, 4>> ParseIpv4(std::string_view value) {
  std::array<unsigned int, 4> octets{};
  std::size_t octetIndex = 0;
  std::string current;
  auto parseOctet = [](const std::string& segment) -> std::optional<unsigned int> {
    try {
      const unsigned long parsed = std::stoul(segment);
      if (parsed > 255UL) return std::nullopt;
      return static_cast<unsigned int>(parsed);
    } catch (...) {
      return std::nullopt;
    }
  };
  for (const char ch : value) {
    if (ch == '.') {
      if (current.empty() || octetIndex >= octets.size()) return std::nullopt;
      const auto parsed = parseOctet(current);
      if (!parsed.has_value()) return std::nullopt;
      octets[octetIndex++] = *parsed;
      current.clear();
      continue;
    }
    if (!std::isdigit(static_cast<unsigned char>(ch))) return std::nullopt;
    current.push_back(ch);
  }
  if (current.empty() || octetIndex != 3) return std::nullopt;
  const auto parsed = parseOctet(current);
  if (!parsed.has_value()) return std::nullopt;
  octets[octetIndex] = *parsed;
  return octets;
}

std::string DescribeCandidate(const EndpointProbeCandidate& candidate) {
  std::ostringstream oss;
  oss << (candidate.interfaceName.empty() ? "unknown" : candidate.interfaceName)
      << " -> " << candidate.hostIp;
  if (!candidate.mode.empty()) oss << " (" << candidate.mode << ")";
  return oss.str();
}

} // namespace

std::string TrimNetworkText(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
  return value.substr(start);
}

bool IsAutoEndpointValue(std::string_view value) {
  return ToLower(TrimNetworkText(std::string(value))) == kAutoAddressToken || TrimNetworkText(std::string(value)).empty();
}

std::vector<std::string> SplitEndpointEntries(std::string_view value) {
  std::vector<std::string> entries;
  std::string current;
  auto flush = [&]() {
    const auto trimmed = TrimNetworkText(current);
    if (!trimmed.empty()) entries.push_back(trimmed);
    current.clear();
  };
  for (char ch : value) {
    if (ch == ',' || ch == ';' || ch == '\n' || ch == '\r' || ch == '\t') {
      flush();
    } else {
      current.push_back(ch);
    }
  }
  flush();
  return entries;
}

std::string FirstEndpointEntryOr(std::string_view value, std::string_view fallback) {
  const auto entries = SplitEndpointEntries(value);
  return entries.empty() ? std::string(fallback) : entries.front();
}

bool LooksLikePrivateIpv4(std::string_view value) {
  const auto parsed = ParseIpv4(value);
  if (!parsed.has_value()) return false;
  const auto& octets = *parsed;
  return octets[0] == 10 ||
         (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) ||
         (octets[0] == 192 && octets[1] == 168);
}

bool LooksLikeApipa(std::string_view value) {
  const auto parsed = ParseIpv4(value);
  return parsed.has_value() && (*parsed)[0] == 169 && (*parsed)[1] == 254;
}

int ScoreEndpointProbeCandidate(const EndpointProbeCandidate& candidate) {
  int score = candidate.scoreAdjustment;

  if (candidate.isPrivateIpv4) score += 200;
  else score += 20;

  if (candidate.isWifi) score += 40;
  if (candidate.hasGateway) score += 20;
  if (candidate.ipv4Enabled) score += 10;
  if (candidate.isUp) score += 100;
  if (candidate.isRunning) score += 40;

  const std::string combined = ToLower(candidate.interfaceName + " " + candidate.interfaceDescription + " " + candidate.mode);
  if (combined.find("wi-fi direct") != std::string::npos ||
      combined.find("wifi direct") != std::string::npos ||
      combined.find("miracast") != std::string::npos) {
    score += 60;
  }
  if (combined.find("mobile hotspot") != std::string::npos ||
      combined.find("hotspot") != std::string::npos ||
      combined.find("hosted") != std::string::npos) {
    score += 50;
  }

  if (candidate.isVirtual ||
      combined.find("virtual") != std::string::npos ||
      combined.find("hyper-v") != std::string::npos ||
      combined.find("vmware") != std::string::npos ||
      combined.find("vethernet") != std::string::npos ||
      combined.find("virtualbox") != std::string::npos ||
      combined.find("wsl") != std::string::npos) {
    score -= 120;
  }

  if (candidate.isTunnel ||
      candidate.isLoopback ||
      combined.find("bluetooth") != std::string::npos ||
      combined.find("loopback") != std::string::npos ||
      combined.find("teredo") != std::string::npos ||
      combined.find("pseudo") != std::string::npos ||
      combined.find("isatap") != std::string::npos ||
      combined.find("vpn") != std::string::npos ||
      combined.find("tap") != std::string::npos ||
      combined.find("tun") != std::string::npos) {
    score -= 80;
  }

  if (candidate.isApipa) score -= 200;
  return score;
}

bool SelectPreferredNetworkInfo(const EndpointProbeResult& probe, NetworkInfo& out, std::string& detail) {
  out = {};
  detail.clear();

  const EndpointProbeCandidate* best = nullptr;
  int bestScore = -100000;
  for (const auto& candidate : probe.candidates) {
    if (candidate.hostIp.empty()) continue;
    const int score = ScoreEndpointProbeCandidate(candidate);
    if (!best || score > bestScore) {
      best = &candidate;
      bestScore = score;
    }
  }

  if (!best) {
    detail = probe.detail.empty() ? "No active IPv4 adapter was detected." : probe.detail;
    return false;
  }

  out.hostIp = best->hostIp;
  out.mode = best->mode.empty() ? "lan" : best->mode;
  detail = probe.detail.empty()
      ? "Auto-discovered host IP from " + DescribeCandidate(*best)
      : probe.detail + " Selected " + DescribeCandidate(*best);
  return true;
}

EndpointSelection ResolveEndpointSelection(const EndpointSelectionRequest& request,
                                           const EndpointProbeResult* probe) {
  EndpointSelection resolution;
  resolution.bindAddress = IsAutoEndpointValue(request.bindAddress) ? "0.0.0.0" : TrimNetworkText(request.bindAddress);

  if (!IsAutoEndpointValue(request.advertiseAddress)) {
    resolution.advertiseAddress = TrimNetworkText(request.advertiseAddress);
    resolution.preferredHost = FirstEndpointEntryOr(request.advertiseAddress, resolution.bindAddress);
    resolution.detail = "Using explicit advertise address from CLI arguments.";
    return resolution;
  }

  NetworkInfo info;
  std::string detail;
  if (probe && SelectPreferredNetworkInfo(*probe, info, detail) && !info.hostIp.empty()) {
    resolution.advertiseAddress = info.hostIp;
    resolution.preferredHost = info.hostIp;
    resolution.usedAutoDiscovery = true;
    resolution.networkInfoAvailable = true;
    resolution.networkInfo = info;
    resolution.detail = detail;
    return resolution;
  }

  resolution.advertiseAddress = "127.0.0.1";
  resolution.preferredHost = "127.0.0.1";
  resolution.detail = (probe && !probe->detail.empty())
      ? probe->detail + " Falling back to loopback advertise address."
      : "Falling back to loopback advertise address because network discovery is unavailable.";
  return resolution;
}

} // namespace lan::network
