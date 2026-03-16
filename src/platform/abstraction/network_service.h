#pragma once

#include "core/network/endpoint_selection.h"

namespace lan::platform {

using ServerEndpointRequest = lan::network::EndpointSelectionRequest;
using ServerEndpointResolution = lan::network::EndpointSelection;
inline constexpr const char* kAutoAddressToken = lan::network::kAutoAddressToken;

class INetworkService {
public:
  virtual ~INetworkService() = default;

  virtual const char* ProviderName() const = 0;
  virtual bool ResolveServerEndpoints(const ServerEndpointRequest& request,
                                      ServerEndpointResolution& resolution,
                                      std::string& err) = 0;
};

} // namespace lan::platform
