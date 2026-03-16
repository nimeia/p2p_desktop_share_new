#pragma once

#include "core/network/endpoint_selection.h"

#include <string>

namespace lan::platform::posix {

bool ProbeNetworkInterfaces(lan::network::EndpointProbeResult& out, std::string& err);

} // namespace lan::platform::posix
