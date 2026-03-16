#include "core/runtime/admin_shell_coordinator.h"

namespace lan::runtime {
namespace {

bool TryMapHostAction(ShellBridgeAdminCommandKind kind, HostActionKind& action) {
  switch (kind) {
    case ShellBridgeAdminCommandKind::StartServer:
      action = HostActionKind::StartServer;
      return true;
    case ShellBridgeAdminCommandKind::StopServer:
      action = HostActionKind::StopServer;
      return true;
    case ShellBridgeAdminCommandKind::ServiceOnly:
      action = HostActionKind::StartServiceOnly;
      return true;
    case ShellBridgeAdminCommandKind::StartAndOpenHost:
      action = HostActionKind::StartAndOpenHost;
      return true;
    case ShellBridgeAdminCommandKind::OpenHost:
      action = HostActionKind::OpenHostPage;
      return true;
    case ShellBridgeAdminCommandKind::OpenViewer:
      action = HostActionKind::OpenViewerPage;
      return true;
    case ShellBridgeAdminCommandKind::ExportBundle:
      action = HostActionKind::ExportShareBundle;
      return true;
    case ShellBridgeAdminCommandKind::OpenOutput:
      action = HostActionKind::OpenOutputFolder;
      return true;
    case ShellBridgeAdminCommandKind::OpenReport:
      action = HostActionKind::OpenDiagnosticsReport;
      return true;
    case ShellBridgeAdminCommandKind::RefreshBundle:
      action = HostActionKind::RefreshDiagnosticsBundle;
      return true;
    case ShellBridgeAdminCommandKind::ShowShareWizard:
      action = HostActionKind::ShowShareWizard;
      return true;
    case ShellBridgeAdminCommandKind::ShowQr:
      action = HostActionKind::ShowQr;
      return true;
    default:
      break;
  }
  return false;
}

void HandleAdminCommand(const ShellBridgeAdminCommand& command,
                        const AdminShellCoordinatorHooks& hooks,
                        AdminShellCoordinatorResult& result) {
  HostActionKind hostAction = HostActionKind::StartServer;
  if (TryMapHostAction(command.kind, hostAction)) {
    if (hooks.executeHostAction) hooks.executeHostAction(hostAction);
    return;
  }

  switch (command.kind) {
    case ShellBridgeAdminCommandKind::RefreshNetwork:
      if (hooks.refreshRuntime) hooks.refreshRuntime();
      break;
    case ShellBridgeAdminCommandKind::GenerateRoomToken:
      if (hooks.generateRoomToken) hooks.generateRoomToken();
      break;
    case ShellBridgeAdminCommandKind::ApplySession:
      if (hooks.applySessionConfig) {
        hooks.applySessionConfig(AdminShellSessionRequest{command.room, command.token, command.bind, command.port});
      }
      break;
    case ShellBridgeAdminCommandKind::CopyHostUrl:
      if (hooks.copyHostUrl) hooks.copyHostUrl();
      break;
    case ShellBridgeAdminCommandKind::CopyViewerUrl:
      if (hooks.copyViewerUrl) hooks.copyViewerUrl();
      break;
    case ShellBridgeAdminCommandKind::QuickFixNetwork:
      if (hooks.quickFixNetwork) hooks.quickFixNetwork();
      break;
    case ShellBridgeAdminCommandKind::QuickFixCertificate:
      if (hooks.quickFixCertificate) hooks.quickFixCertificate();
      break;
    case ShellBridgeAdminCommandKind::QuickFixSharing:
      if (hooks.quickFixSharing) hooks.quickFixSharing();
      break;
    case ShellBridgeAdminCommandKind::QuickFixHandoff:
      if (hooks.quickFixHandoff) hooks.quickFixHandoff();
      break;
    case ShellBridgeAdminCommandKind::QuickFixHotspot:
      if (hooks.quickFixHotspot) hooks.quickFixHotspot();
      break;
    case ShellBridgeAdminCommandKind::SelectAdapter:
      if (hooks.selectNetworkCandidate && command.hasIndex) hooks.selectNetworkCandidate(command.index);
      break;
    case ShellBridgeAdminCommandKind::ApplyHotspot:
      if (hooks.applyHotspotConfig) hooks.applyHotspotConfig(AdminShellHotspotRequest{command.ssid, command.password});
      break;
    case ShellBridgeAdminCommandKind::StartHotspot:
      if (hooks.startHotspot) hooks.startHotspot();
      break;
    case ShellBridgeAdminCommandKind::StopHotspot:
      if (hooks.stopHotspot) hooks.stopHotspot();
      break;
    case ShellBridgeAdminCommandKind::AutoHotspot:
      if (hooks.autoHotspot) hooks.autoHotspot();
      break;
    case ShellBridgeAdminCommandKind::OpenHotspotSettings:
      if (hooks.openHotspotSettings) hooks.openHotspotSettings();
      break;
    case ShellBridgeAdminCommandKind::OpenFirewallSettings:
      if (hooks.openFirewallSettings) hooks.openFirewallSettings();
      break;
    case ShellBridgeAdminCommandKind::RunNetworkDiagnostics:
      if (hooks.runNetworkDiagnostics) hooks.runNetworkDiagnostics();
      break;
    case ShellBridgeAdminCommandKind::CheckWebViewRuntime:
      if (hooks.checkWebViewRuntime) hooks.checkWebViewRuntime();
      break;
    case ShellBridgeAdminCommandKind::TrustLocalCertificate:
      if (hooks.trustLocalCertificate) hooks.trustLocalCertificate();
      break;
    case ShellBridgeAdminCommandKind::ExportRemoteProbeGuide:
      if (hooks.exportRemoteProbeGuide) hooks.exportRemoteProbeGuide();
      break;
    case ShellBridgeAdminCommandKind::OpenConnectedDevices:
      if (hooks.openConnectedDevices) hooks.openConnectedDevices();
      break;
    case ShellBridgeAdminCommandKind::SwitchPage:
      if (hooks.navigatePage && !command.page.empty()) {
        hooks.navigatePage(command.page);
      } else {
        result.stateChanged = false;
      }
      break;
    case ShellBridgeAdminCommandKind::None:
    default:
      result.stateChanged = false;
      result.logLine = L"Admin shell unknown command: " + command.commandName;
      break;
  }
}

} // namespace

AdminShellCoordinatorResult HandleAdminShellMessage(const ShellBridgeInboundMessage& message,
                                                    const AdminShellCoordinatorHooks& hooks) {
  AdminShellCoordinatorResult result;

  if (message.source != ShellBridgeSource::AdminShell) {
    result.logLine = L"Admin shell ignored message: " + message.rawPayload;
    return result;
  }

  switch (message.kind) {
    case ShellBridgeInboundKind::AdminReady:
    case ShellBridgeInboundKind::AdminRequestSnapshot:
      result.requestSnapshot = true;
      return result;
    case ShellBridgeInboundKind::AdminCommand:
      result.stateChanged = true;
      HandleAdminCommand(message.adminCommand, hooks, result);
      return result;
    case ShellBridgeInboundKind::Unknown:
      result.logLine = L"Admin shell ignored message: " + message.rawPayload;
      return result;
    case ShellBridgeInboundKind::None:
    case ShellBridgeInboundKind::HostStatus:
    case ShellBridgeInboundKind::HostLog:
    default:
      result.logLine = L"Admin shell ignored message: " + message.rawPayload;
      return result;
  }
}

} // namespace lan::runtime
