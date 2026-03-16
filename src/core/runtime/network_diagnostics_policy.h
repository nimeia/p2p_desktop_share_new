#pragma once

#include "core/runtime/runtime_controller.h"

#include <string>

namespace lan::runtime {

struct NetworkDiagnosticsViewModel {
  bool firewallReady = false;
  std::wstring firewallLabel;
  std::wstring firewallDetail;
  std::wstring firewallAction;

  bool remoteViewerReady = false;
  std::wstring remoteViewerLabel;
  std::wstring remoteViewerDetail;
  std::wstring remoteViewerAction;
};

NetworkDiagnosticsViewModel BuildNetworkDiagnosticsViewModel(const RuntimeSessionState& session,
                                                             const RuntimeHealthState& health);

} // namespace lan::runtime
