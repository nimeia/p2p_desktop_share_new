#pragma once

#include <cstdint>

namespace lan::runtime {

struct HostRuntimeSchedulerConfig {
  unsigned int tickIntervalMs = 1000;
  bool refreshUiEveryTick = true;
  bool pollWhenServerRunning = true;
};

struct HostRuntimeTickState {
  std::uint64_t tickCount = 0;
};

struct HostRuntimeTickInput {
  bool serverRunning = false;
  bool pollInFlight = false;
  bool forceUiRefresh = false;
  bool forcePoll = false;
};

struct HostRuntimeTickResult {
  HostRuntimeTickState nextState;
  bool refreshUi = false;
  bool kickPoll = false;
  unsigned int nextTickIntervalMs = 1000;
};

HostRuntimeSchedulerConfig DefaultHostRuntimeSchedulerConfig();
HostRuntimeTickResult CoordinateHostRuntimeTick(const HostRuntimeTickState& state,
                                               const HostRuntimeTickInput& input,
                                               const HostRuntimeSchedulerConfig& config = DefaultHostRuntimeSchedulerConfig());

} // namespace lan::runtime
