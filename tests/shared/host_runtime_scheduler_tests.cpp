#include "core/runtime/host_runtime_scheduler.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "host runtime scheduler test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  const auto config = DefaultHostRuntimeSchedulerConfig();
  Expect(config.tickIntervalMs == 1000, "default tick interval should stay at 1000ms");
  Expect(config.refreshUiEveryTick, "default scheduler should refresh UI every tick");
  Expect(config.pollWhenServerRunning, "default scheduler should poll when server is running");

  HostRuntimeTickState state;

  HostRuntimeTickInput first;
  first.serverRunning = true;
  auto firstResult = CoordinateHostRuntimeTick(state, first);
  Expect(firstResult.nextState.tickCount == 1, "first tick should increment tick count");
  Expect(firstResult.refreshUi, "first tick should request ui refresh");
  Expect(firstResult.kickPoll, "first tick should kick poll when server is running");

  HostRuntimeTickInput inflight;
  inflight.serverRunning = true;
  inflight.pollInFlight = true;
  auto inflightResult = CoordinateHostRuntimeTick(firstResult.nextState, inflight);
  Expect(inflightResult.nextState.tickCount == 2, "second tick should increment tick count");
  Expect(inflightResult.refreshUi, "ui refresh should still happen when poll is in flight");
  Expect(!inflightResult.kickPoll, "poll should be skipped while another poll is in flight");

  HostRuntimeTickInput stopped;
  stopped.serverRunning = false;
  auto stoppedResult = CoordinateHostRuntimeTick(inflightResult.nextState, stopped);
  Expect(stoppedResult.nextState.tickCount == 3, "third tick should increment tick count");
  Expect(stoppedResult.refreshUi, "stopped server should still refresh ui");
  Expect(!stoppedResult.kickPoll, "stopped server should not kick poll");

  HostRuntimeTickInput forced;
  forced.serverRunning = false;
  forced.forcePoll = true;
  auto forcedResult = CoordinateHostRuntimeTick(stoppedResult.nextState, forced);
  Expect(forcedResult.kickPoll, "forcePoll should override server-running guard");

  std::cout << "host runtime scheduler tests passed\n";
  return 0;
}
