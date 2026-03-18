#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include "core/runtime/host_action_coordinator.h"
#include "core/runtime/shell_bridge_presenter.h"

namespace lan::runtime {

struct AdminShellSessionRequest {
  std::wstring room;
  std::wstring token;
  std::wstring bindAddress;
  int port = 9443;
};

struct AdminShellHotspotRequest {
  std::wstring ssid;
  std::wstring password;
};

struct AdminShellCoordinatorHooks {
  std::function<void()> refreshRuntime;
  std::function<void()> generateRoomToken;
  std::function<void(const AdminShellSessionRequest&)> applySessionConfig;
  std::function<void(const AdminShellHotspotRequest&)> applyHotspotConfig;
  std::function<void(HostActionKind)> executeHostAction;
  std::function<void()> copyHostUrl;
  std::function<void()> copyViewerUrl;
  std::function<void()> quickFixNetwork;
  std::function<void()> quickFixCertificate;
  std::function<void()> quickFixSharing;
  std::function<void()> quickFixHandoff;
  std::function<void()> quickFixHotspot;
  std::function<void(std::size_t index)> selectNetworkCandidate;
  std::function<void()> startHotspot;
  std::function<void()> stopHotspot;
  std::function<void()> autoHotspot;
  std::function<void()> openHotspotSettings;
  std::function<void()> openFirewallSettings;
  std::function<void()> runNetworkDiagnostics;
  std::function<void()> checkWebViewRuntime;
  std::function<void()> trustLocalCertificate;
  std::function<void()> exportRemoteProbeGuide;
  std::function<void()> openConnectedDevices;
  std::function<void(std::wstring page)> navigatePage;
  std::function<void(std::wstring locale)> setLanguage;
};

struct AdminShellCoordinatorResult {
  bool requestSnapshot = false;
  bool stateChanged = false;
  std::wstring logLine;
};

AdminShellCoordinatorResult HandleAdminShellMessage(const ShellBridgeInboundMessage& message,
                                                    const AdminShellCoordinatorHooks& hooks);

} // namespace lan::runtime
