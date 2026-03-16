#pragma once

#include "core/network/endpoint_selection.h"

#include <string>

namespace lan::platform::windows {

bool ProbeNetworkInterfaces(lan::network::EndpointProbeResult& out, std::string& err);

} // namespace lan::platform::windows
