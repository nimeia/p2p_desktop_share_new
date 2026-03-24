#include "core/network/endpoint_selection.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "network test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::network;

  Expect(IsAutoEndpointValue("auto"), "auto token should be recognized");
  Expect(IsAutoEndpointValue(" AUTO \n"), "auto token should be trimmed and case-insensitive");
  Expect(!IsAutoEndpointValue("192.168.1.20"), "explicit host should not be treated as auto");

  const auto entries = SplitEndpointEntries(" 192.168.1.20, example.local ; 10.0.0.5 ");
  Expect(entries.size() == 3, "entry splitting should preserve three values");
  Expect(entries.front() == "192.168.1.20", "first entry should be trimmed");
  Expect(FirstEndpointEntryOr("example.local,10.0.0.5", "0.0.0.0") == "example.local", "first entry should be returned");

  EndpointProbeResult probe;
  probe.detail = "Probe completed.";
  probe.candidates.push_back(EndpointProbeCandidate{
      .interfaceName = "vEthernet (WSL)",
      .interfaceDescription = "Hyper-V Virtual Ethernet Adapter",
      .mode = "lan",
      .hostIp = "172.30.96.1",
      .isUp = true,
      .isRunning = true,
      .isVirtual = true,
      .isPrivateIpv4 = true,
      .ipv4Enabled = true,
  });
  probe.candidates.push_back(EndpointProbeCandidate{
      .interfaceName = "Wi-Fi",
      .interfaceDescription = "Intel Wireless",
      .mode = "wifi",
      .hostIp = "192.168.50.23",
      .isUp = true,
      .isRunning = true,
      .isPrivateIpv4 = true,
      .hasGateway = true,
      .isWifi = true,
      .ipv4Enabled = true,
  });

  NetworkInfo info;
  std::string detail;
  Expect(SelectPreferredNetworkInfo(probe, info, detail), "selection should succeed when candidates exist");
  Expect(info.hostIp == "192.168.50.23", "physical Wi-Fi adapter should outrank virtual adapter");
  Expect(info.mode == "wifi", "selected mode should propagate");

  EndpointSelection explicitSelection = ResolveEndpointSelection(
      EndpointSelectionRequest{.bindAddress = "0.0.0.0", .advertiseAddress = "example.local,10.0.0.5"},
      &probe);
  Expect(explicitSelection.advertiseAddress == "example.local,10.0.0.5", "explicit advertise address should be preserved");
  Expect(explicitSelection.preferredHost == "example.local", "preferred host should use first explicit entry");
  Expect(!explicitSelection.usedAutoDiscovery, "explicit advertise address should bypass auto discovery");

  EndpointSelection autoSelection = ResolveEndpointSelection(EndpointSelectionRequest{}, &probe);
  Expect(autoSelection.usedAutoDiscovery, "auto selection should use probe results");
  Expect(autoSelection.preferredHost == "192.168.50.23", "auto selection should use preferred probe host");

  EndpointSelection fallbackSelection = ResolveEndpointSelection(EndpointSelectionRequest{}, nullptr);
  Expect(fallbackSelection.preferredHost == "127.0.0.1", "missing probe should fall back to loopback");
  Expect(!fallbackSelection.usedAutoDiscovery, "fallback should not mark auto discovery");

  std::cout << "network tests passed\n";
  return 0;
}
