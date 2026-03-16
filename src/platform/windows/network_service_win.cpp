#include "platform/windows/network_service_win.h"

#include "platform/windows/network_probe_win.h"

namespace lan::platform::windows {

const char* WindowsNetworkService::ProviderName() const {
  return "windows-network-service";
}

bool WindowsNetworkService::ResolveServerEndpoints(const ServerEndpointRequest& request,
                                                   ServerEndpointResolution& resolution,
                                                   std::string& err) {
  err.clear();
  lan::network::EndpointProbeResult probe;
  std::string probeErr;
  ProbeNetworkInterfaces(probe, probeErr);
  resolution = lan::network::ResolveEndpointSelection(request, &probe);
  err.clear();
  return true;
}

} // namespace lan::platform::windows
