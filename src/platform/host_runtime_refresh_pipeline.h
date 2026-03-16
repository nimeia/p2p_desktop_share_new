#pragma once

#include "core/runtime/host_runtime_coordinator.h"

namespace lan::platform {

class PlatformServiceFacade;

lan::runtime::HostRuntimeRefreshResult RunHostRuntimeRefreshPipeline(const PlatformServiceFacade* services,
                                                                    const lan::runtime::HostRuntimeRefreshInput& baseInput);

} // namespace lan::platform
