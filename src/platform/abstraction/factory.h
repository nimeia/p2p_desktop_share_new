#pragma once

#include "platform/abstraction/cert_provider.h"
#include "platform/abstraction/network_service.h"
#include "platform/abstraction/system_actions.h"

#include <memory>

namespace lan::platform {

class PlatformServiceFacade;

std::unique_ptr<ICertProvider> CreateDefaultCertProvider();
std::unique_ptr<INetworkService> CreateDefaultNetworkService();
std::unique_ptr<ISystemActions> CreateDefaultSystemActions();
std::unique_ptr<PlatformServiceFacade> CreateDefaultPlatformServiceFacade();

} // namespace lan::platform
