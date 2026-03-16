#include "host_shell/native_shell_runtime_loop.h"

namespace lan::host_shell {
namespace {

std::string Narrow(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  return out;
}

} // namespace

NativeShellRuntimeLoop::NativeShellRuntimeLoop(NativeShellPollFunction poll,
                                               lan::platform::PlatformServiceFacade& facade,
                                               lan::runtime::NativeShellAlertDebounceConfig debounce)
    : poll_(std::move(poll)), facade_(facade), debounce_(debounce) {}

NativeShellRuntimeLoopResult NativeShellRuntimeLoop::Tick() {
  NativeShellRuntimeLoopResult result;
  result.snapshot = poll_();
  result.tracker = lan::runtime::TickNativeShellStatusTracker(result.snapshot.runtime, memory_, debounce_);
  memory_ = result.tracker.memory;

  for (const auto& notification : result.tracker.notifications) {
    std::string err;
    if (!facade_.ShowNotification(Narrow(notification.title), Narrow(notification.body), err) && !err.empty()) {
      result.notificationErrors.push_back(err);
    }
  }
  return result;
}

NativeShellPollFunction MakeNativeShellPollFunction(NativeShellEndpointConfig config) {
  return [config]() { return PollNativeShellLive(config); };
}

} // namespace lan::host_shell
