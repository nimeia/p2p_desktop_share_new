#include "platform/host_runtime_refresh_pipeline.h"

#include "platform/abstraction/platform_service_facade.h"

namespace lan::platform {

lan::runtime::HostRuntimeRefreshResult RunHostRuntimeRefreshPipeline(const PlatformServiceFacade* services,
                                                                    const lan::runtime::HostRuntimeRefreshInput& baseInput) {
  auto input = baseInput;
  if (services) {
    std::string err;
    lan::network::NetworkInfo info;
    if (services->GetCurrentNetworkInfo(info, err)) {
      input.networkInfoAvailable = !info.hostIp.empty();
      input.networkInfo = std::move(info);
    } else {
      input.networkInfoError = std::move(err);
    }

    err.clear();
    lan::network::NetworkCapabilities caps;
    if (services->QueryNetworkCapabilities(caps, err)) {
      input.capabilitiesAvailable = true;
      input.capabilities = std::move(caps);
    } else {
      input.capabilitiesError = std::move(err);
    }

    err.clear();
    lan::network::HotspotState state;
    if (services->QueryHotspotState(state, err)) {
      input.hotspotStateAvailable = true;
      input.hotspotState = std::move(state);
    } else {
      input.hotspotStateError = std::move(err);
    }
  }
  return lan::runtime::CoordinateHostRuntimeRefresh(input);
}

} // namespace lan::platform
