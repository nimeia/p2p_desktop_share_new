#include "platform/posix/network_service_posix.h"

#include "platform/posix/network_probe_posix.h"

namespace lan::platform::posix {

const char* PosixNetworkService::ProviderName() const {
  return "posix-network-service";
}

bool PosixNetworkService::ResolveServerEndpoints(const ServerEndpointRequest& request,
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

} // namespace lan::platform::posix
