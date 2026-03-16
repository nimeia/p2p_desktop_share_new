#pragma once

#include "platform/abstraction/network_service.h"

namespace lan::platform::windows {

class WindowsNetworkService final : public INetworkService {
public:
  const char* ProviderName() const override;
  bool ResolveServerEndpoints(const ServerEndpointRequest& request,
                              ServerEndpointResolution& resolution,
                              std::string& err) override;
};

} // namespace lan::platform::windows
