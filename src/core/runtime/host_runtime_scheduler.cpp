#include "core/runtime/host_runtime_scheduler.h"

namespace lan::runtime {

HostRuntimeSchedulerConfig DefaultHostRuntimeSchedulerConfig() {
  return HostRuntimeSchedulerConfig{};
}

HostRuntimeTickResult CoordinateHostRuntimeTick(const HostRuntimeTickState& state,
                                               const HostRuntimeTickInput& input,
                                               const HostRuntimeSchedulerConfig& config) {
  HostRuntimeTickResult result;
  result.nextState = state;
  ++result.nextState.tickCount;
  result.nextTickIntervalMs = config.tickIntervalMs;
  result.refreshUi = config.refreshUiEveryTick || input.forceUiRefresh;
  result.kickPoll = (config.pollWhenServerRunning && input.serverRunning && !input.pollInFlight) || input.forcePoll;
  return result;
}

} // namespace lan::runtime
