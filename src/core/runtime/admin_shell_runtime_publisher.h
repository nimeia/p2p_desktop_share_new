#pragma once

#include <functional>
#include <string>

#include "core/runtime/admin_view_model_assembler.h"
#include "core/runtime/shell_bridge_presenter.h"

namespace lan::runtime {

struct AdminShellRuntimeRefreshPolicy {
  bool markShellReady = false;
  bool shouldPublish = false;
};

struct AdminShellRuntimePublishContext {
  bool adminShellActive = false;
  bool adminShellReady = false;
  AdminViewModelInput viewModelInput;
};

struct AdminShellRuntimePublisherHooks {
  std::function<void(const std::wstring&)> publishJson;
};

struct AdminShellRuntimePublishResult {
  bool published = false;
  ShellBridgeSnapshotState snapshot;
  std::wstring eventJson;
};

AdminShellRuntimeRefreshPolicy ResolveAdminShellRuntimeRefreshPolicy(bool requestSnapshot, bool stateChanged);
ShellBridgeSnapshotState BuildAdminShellSnapshotState(const AdminViewModelInput& input);
AdminShellRuntimePublishResult PublishAdminShellRuntime(const AdminShellRuntimePublishContext& context,
                                                        const AdminShellRuntimePublisherHooks& hooks);

} // namespace lan::runtime
