#pragma once

#include "core/runtime/native_shell_status_tracker.h"
#include "host_shell/native_shell_live_poller.h"
#include "platform/abstraction/platform_service_facade.h"

#include <functional>
#include <memory>
#include <string>

namespace lan::host_shell {

using NativeShellPollFunction = std::function<NativeShellLiveSnapshot()>;

struct NativeShellRuntimeLoopResult {
  NativeShellLiveSnapshot snapshot;
  lan::runtime::NativeShellStatusTrackerResult tracker;
  std::vector<std::string> notificationErrors;
};

class NativeShellRuntimeLoop {
public:
  NativeShellRuntimeLoop(NativeShellPollFunction poll,
                         lan::platform::PlatformServiceFacade& facade,
                         lan::runtime::NativeShellAlertDebounceConfig debounce = {});

  NativeShellRuntimeLoopResult Tick();

private:
  NativeShellPollFunction poll_;
  lan::platform::PlatformServiceFacade& facade_;
  lan::runtime::NativeShellAlertDebounceConfig debounce_;
  lan::runtime::NativeShellAlertMemory memory_;
};

NativeShellPollFunction MakeNativeShellPollFunction(NativeShellEndpointConfig config);

} // namespace lan::host_shell
