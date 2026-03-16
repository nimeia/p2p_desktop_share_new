#include "core/runtime/host_action_coordinator.h"

#include <utility>

namespace lan::runtime {
namespace {

HostActionResult FailureResult(std::wstring message) {
  HostActionResult result;
  result.ok = false;
  result.logs.push_back(std::move(message));
  return result;
}

void AppendFrom(HostActionResult& dst, HostActionResult src) {
  dst.ok = src.ok;
  dst.performed = dst.performed || src.performed;
  dst.logs.insert(dst.logs.end(), src.logs.begin(), src.logs.end());
  dst.timelineEvents.insert(dst.timelineEvents.end(), src.timelineEvents.begin(), src.timelineEvents.end());
  if (!src.paths.shareCardPath.empty()) dst.paths.shareCardPath = std::move(src.paths.shareCardPath);
  if (!src.paths.shareWizardPath.empty()) dst.paths.shareWizardPath = std::move(src.paths.shareWizardPath);
  if (!src.paths.bundleJsonPath.empty()) dst.paths.bundleJsonPath = std::move(src.paths.bundleJsonPath);
  if (!src.paths.desktopSelfCheckPath.empty()) dst.paths.desktopSelfCheckPath = std::move(src.paths.desktopSelfCheckPath);
  dst.effects.handoffStarted = dst.effects.handoffStarted || src.effects.handoffStarted;
  dst.effects.shareWizardOpened = dst.effects.shareWizardOpened || src.effects.shareWizardOpened;
  dst.effects.shareCardExported = dst.effects.shareCardExported || src.effects.shareCardExported;
  dst.effects.clearHandoffDelivered = dst.effects.clearHandoffDelivered || src.effects.clearHandoffDelivered;
  dst.effects.refreshShareInfo = dst.effects.refreshShareInfo || src.effects.refreshShareInfo;
  dst.effects.refreshDashboard = dst.effects.refreshDashboard || src.effects.refreshDashboard;
  if (src.effects.setPage) {
    dst.effects.setPage = true;
    dst.effects.page = src.effects.page;
  }
}

HostActionResult ExecuteStartServer(const HostActionHooks& hooks) {
  if (!hooks.startServer) return FailureResult(L"Start action is unavailable");
  const auto op = hooks.startServer();

  HostActionResult result;
  result.ok = op.ok;
  result.performed = op.performed;
  result.effects.refreshShareInfo = true;
  if (op.ok) {
    if (op.performed) {
      result.logs.push_back(op.detail.empty() ? L"Server started" : op.detail);
      result.timelineEvents.push_back(L"Service started");
    } else if (!op.detail.empty()) {
      result.logs.push_back(op.detail);
    }
  } else {
    result.logs.push_back(op.detail.empty() ? L"Start failed" : op.detail);
  }
  return result;
}

HostActionResult ExecuteStopServer(const HostActionHooks& hooks) {
  if (!hooks.stopServer) return FailureResult(L"Stop action is unavailable");
  const auto op = hooks.stopServer();

  HostActionResult result;
  result.ok = op.ok;
  result.performed = op.performed;
  result.effects.refreshShareInfo = true;
  result.effects.clearHandoffDelivered = op.ok && op.performed;
  if (op.ok) {
    if (op.performed) {
      result.logs.push_back(op.detail.empty() ? L"Server stopped" : op.detail);
      result.timelineEvents.push_back(L"Service stopped");
    } else if (!op.detail.empty()) {
      result.logs.push_back(op.detail);
    }
  } else {
    result.logs.push_back(op.detail.empty() ? L"Stop failed" : op.detail);
  }
  return result;
}

HostActionResult ExecuteOpenHostPage(const HostActionHooks& hooks) {
  if (!hooks.openHostPage) return FailureResult(L"Open Host page action is unavailable");
  const auto op = hooks.openHostPage();

  HostActionResult result;
  result.ok = op.ok;
  result.performed = op.performed;
  if (op.ok) {
    if (op.performed) {
      result.logs.push_back(op.detail.empty() ? L"Opened Host page" : op.detail);
      result.timelineEvents.push_back(L"Host page opened");
    } else if (!op.detail.empty()) {
      result.logs.push_back(op.detail);
    }
  } else {
    result.logs.push_back(op.detail.empty() ? L"Open Host page failed" : op.detail);
  }
  return result;
}

HostActionResult ExecuteOpenViewerPage(const HostActionHooks& hooks) {
  if (!hooks.openViewerPage) return FailureResult(L"Open Viewer page action is unavailable");
  const auto op = hooks.openViewerPage();

  HostActionResult result;
  result.ok = op.ok;
  result.performed = op.performed;
  result.effects.handoffStarted = true;
  if (op.ok) {
    if (op.performed) {
      result.logs.push_back(op.detail.empty() ? L"Opened Viewer page" : op.detail);
      result.timelineEvents.push_back(L"Viewer page opened");
    } else if (!op.detail.empty()) {
      result.logs.push_back(op.detail);
    }
  } else {
    result.logs.push_back(op.detail.empty() ? L"Open Viewer page failed" : op.detail);
  }
  return result;
}

HostActionResult ExecuteEnsureArtifacts(const HostActionHooks& hooks,
                                       const HostActionArtifactRequest& request,
                                       HostActionArtifactPaths& paths,
                                       std::wstring failureMessage) {
  if (!hooks.ensureArtifacts) return FailureResult(std::move(failureMessage));
  const auto op = hooks.ensureArtifacts(request, paths);

  HostActionResult result;
  result.ok = op.ok;
  result.performed = op.performed;
  result.paths = paths;
  if (!op.ok) {
    result.logs.push_back(op.detail.empty() ? std::move(failureMessage) : op.detail);
  }
  return result;
}

HostActionResult ExecuteOpenPath(const HostActionHooks& hooks,
                                 const std::filesystem::path& path,
                                 std::wstring failureMessage) {
  if (!hooks.openPath) return FailureResult(std::move(failureMessage));
  const auto op = hooks.openPath(path);

  HostActionResult result;
  result.ok = op.ok;
  result.performed = op.performed;
  if (op.ok) {
    if (op.performed && !op.detail.empty()) {
      result.logs.push_back(op.detail);
    }
  } else {
    result.logs.push_back(op.detail.empty() ? std::move(failureMessage) : op.detail);
  }
  return result;
}

} // namespace

HostActionResult ExecuteHostAction(HostActionKind kind,
                                   const HostActionContext& context,
                                   const HostActionHooks& hooks) {
  switch (kind) {
    case HostActionKind::StartServer:
    case HostActionKind::StartServiceOnly:
      return ExecuteStartServer(hooks);

    case HostActionKind::StopServer:
      return ExecuteStopServer(hooks);

    case HostActionKind::RestartServer: {
      HostActionResult result;
      AppendFrom(result, ExecuteStopServer(hooks));
      if (!result.ok) return result;
      AppendFrom(result, ExecuteStartServer(hooks));
      return result;
    }

    case HostActionKind::StartAndOpenHost: {
      HostActionResult result;
      AppendFrom(result, ExecuteStartServer(hooks));
      if (!result.ok) return result;
      AppendFrom(result, ExecuteOpenHostPage(hooks));
      return result;
    }

    case HostActionKind::OpenHostPage:
      return ExecuteOpenHostPage(hooks);

    case HostActionKind::OpenViewerPage:
      return ExecuteOpenViewerPage(hooks);

    case HostActionKind::ShowQr: {
      HostActionResult result;
      result.effects.handoffStarted = true;
      HostActionArtifactPaths paths;
      HostActionArtifactRequest request;
      request.shareCard = true;
      AppendFrom(result, ExecuteEnsureArtifacts(hooks, request, paths, L"Open share card failed"));
      if (!result.ok) return result;
      AppendFrom(result, ExecuteOpenPath(hooks, paths.shareCardPath, L"Open share card failed"));
      if (result.ok) {
        result.effects.shareCardExported = true;
        result.effects.refreshDashboard = true;
        result.logs.push_back(L"Opened share card");
      }
      return result;
    }

    case HostActionKind::ShowShareWizard: {
      HostActionResult result;
      result.effects.handoffStarted = true;
      result.effects.shareWizardOpened = true;
      result.effects.refreshShareInfo = true;
      HostActionArtifactPaths paths;
      HostActionArtifactRequest request;
      request.shareWizard = true;
      AppendFrom(result, ExecuteEnsureArtifacts(hooks, request, paths, L"Open share wizard failed"));
      if (!result.ok) return result;
      AppendFrom(result, ExecuteOpenPath(hooks, paths.shareWizardPath, L"Open share wizard failed"));
      if (result.ok) {
        result.timelineEvents.push_back(L"Share Wizard opened");
        result.logs.push_back(L"Opened share wizard");
      }
      return result;
    }

    case HostActionKind::ExportShareBundle: {
      HostActionResult result;
      result.effects.handoffStarted = true;
      result.effects.shareCardExported = true;
      result.effects.refreshDashboard = true;
      HostActionArtifactPaths paths;
      HostActionArtifactRequest request;
      request.bundleJson = true;
      AppendFrom(result, ExecuteEnsureArtifacts(hooks, request, paths, L"Export share bundle failed"));
      if (!result.ok) return result;
      AppendFrom(result, ExecuteOpenPath(hooks, context.outputDir, L"Open bundle folder failed"));
      if (result.ok) {
        result.logs.push_back(L"Exported local share bundle");
        result.timelineEvents.push_back(L"Diagnostics bundle generated");
      }
      return result;
    }

    case HostActionKind::RunDesktopSelfCheck: {
      HostActionResult result;
      HostActionArtifactPaths paths;
      HostActionArtifactRequest request;
      request.desktopSelfCheck = true;
      AppendFrom(result, ExecuteEnsureArtifacts(hooks, request, paths, L"Open desktop self-check failed"));
      if (!result.ok) return result;
      AppendFrom(result, ExecuteOpenPath(hooks, paths.desktopSelfCheckPath, L"Open desktop self-check failed"));
      if (result.ok) {
        result.logs.push_back(L"Opened desktop self-check: " + paths.desktopSelfCheckPath.wstring());
      }
      return result;
    }

    case HostActionKind::RefreshDiagnosticsBundle: {
      HostActionResult result;
      HostActionArtifactPaths paths;
      HostActionArtifactRequest request;
      AppendFrom(result, ExecuteEnsureArtifacts(hooks, request, paths, L"Re-run checks failed while exporting diagnostics bundle"));
      if (result.ok) {
        result.logs.push_back(L"Re-ran exported self-check and diagnostics bundle");
      }
      return result;
    }

    case HostActionKind::OpenDiagnosticsReport: {
      HostActionResult result;
      if (!context.diagnosticsReportExists) {
        HostActionArtifactPaths paths;
        HostActionArtifactRequest request;
        AppendFrom(result, ExecuteEnsureArtifacts(hooks, request, paths, L"Diagnostics report missing"));
        if (!result.ok) return result;
      }
      AppendFrom(result, ExecuteOpenPath(hooks, context.diagnosticsReportPath, L"Open diagnostics report failed"));
      if (result.ok) {
        result.logs.push_back(L"Opened diagnostics report: " + context.diagnosticsReportPath.wstring());
      }
      return result;
    }

    case HostActionKind::OpenOutputFolder:
      return ExecuteOpenPath(hooks, context.outputDir, L"Open bundle folder failed");
  }

  return FailureResult(L"Unknown host action");
}

} // namespace lan::runtime
