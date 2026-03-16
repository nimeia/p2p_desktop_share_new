#pragma once

#include "core/runtime/native_shell_alert_coordinator.h"

#include <string>

namespace lan::host_shell {

struct NativeShellEndpointConfig {
  std::string host = "127.0.0.1";
  int port = 8443;
  int timeoutMs = 1500;
};

struct NativeShellLiveSnapshot {
  lan::runtime::NativeShellRuntimeState runtime;
  std::size_t rooms = 0;
  std::size_t viewers = 0;
  bool statusEndpointReady = false;
  std::string diagnostic;
};

NativeShellLiveSnapshot PollNativeShellLive(const NativeShellEndpointConfig& config);

} // namespace lan::host_shell
