#pragma once

#include <filesystem>
#include <string>

namespace lan::platform::windows {

struct FirewallProbeResult {
  bool ready = false;
  bool firewallEnabled = false;
  bool matchingAppRule = false;
  bool matchingPortRule = false;
  std::wstring detail;
};

FirewallProbeResult ProbeFirewallReadiness(const std::filesystem::path& serverExePath,
                                          int port);

} // namespace lan::platform::windows
