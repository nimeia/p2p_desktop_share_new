#pragma once

#include "core/runtime/admin_view_model_assembler.h"

#include <array>
#include <string>

namespace lan::runtime {

struct MonitorViewModel {
  std::array<std::wstring, 5> metricCards{};
  std::wstring timelineText;
  std::wstring detailText;
};

struct DiagnosticsViewModel {
  std::wstring checklistCard;
  std::wstring actionsCard;
  std::wstring exportCard;
  std::wstring filesCard;
};

struct ShellStateInput {
  bool htmlAdminMode = false;
  bool adminShellReady = false;
  bool serverRunning = false;
  bool uiBundleExists = false;
  std::wstring webviewStatus;
  std::wstring webviewDetail;
};

struct ShellFallbackViewModel {
  bool showFallback = false;
  std::wstring bodyText;
  std::wstring startButtonLabel;
  bool startButtonEnabled = true;
  std::wstring startHostButtonLabel;
};

MonitorViewModel BuildMonitorViewModel(const AdminViewModelInput& input);
DiagnosticsViewModel BuildDiagnosticsViewModel(const AdminViewModelInput& input);
ShellFallbackViewModel BuildShellFallbackViewModel(const ShellStateInput& input);

} // namespace lan::runtime
