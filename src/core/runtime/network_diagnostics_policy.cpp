#include "core/runtime/network_diagnostics_policy.h"

namespace lan::runtime {
namespace {

bool MissingHostIp(std::wstring_view hostIp) {
  return hostIp.empty() || hostIp == L"(not found)" || hostIp == L"0.0.0.0";
}

} // namespace

NetworkDiagnosticsViewModel BuildNetworkDiagnosticsViewModel(const RuntimeSessionState& session,
                                                             const RuntimeHealthState& health) {
  NetworkDiagnosticsViewModel model;

  if (!health.serverProcessRunning) {
    model.firewallReady = false;
    model.firewallLabel = L"Deferred";
    model.firewallDetail = L"The local server is not running, so inbound firewall readiness was not checked yet.";
    model.firewallAction = L"Start the local server, then run diagnostics again.";
  } else if (health.firewallReady) {
    model.firewallReady = true;
    model.firewallLabel = L"Inbound allow path detected";
    model.firewallDetail = health.firewallDetail.empty()
        ? L"Windows Firewall already has an inbound allow path for the local share service or its TCP port."
        : health.firewallDetail;
    model.firewallAction = L"No firewall change is required unless another security product overrides Windows Firewall.";
  } else {
    model.firewallReady = false;
    model.firewallLabel = L"Needs attention";
    model.firewallDetail = health.firewallDetail.empty()
        ? L"Windows Firewall is enabled, but no enabled inbound allow rule was detected for the local share service or its TCP port."
        : health.firewallDetail;
    model.firewallAction = L"Open Windows Firewall settings or create an inbound allow rule for ViewMeshServer.exe / the current TCP port.";
  }

  if (!health.serverProcessRunning) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = L"Server stopped";
    model.remoteViewerDetail = L"Remote viewers cannot reach this host until the local server is running.";
    model.remoteViewerAction = L"Start the local server first.";
    return model;
  }

  if (!health.portReady) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = L"Port blocked";
    model.remoteViewerDetail = health.portDetail.empty()
        ? L"The configured TCP port is not ready, so remote viewers cannot connect to this session."
        : health.portDetail;
    model.remoteViewerAction = L"Free or change the configured port, then restart the server.";
    return model;
  }

  if (!health.lanBindReady) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = L"Loopback-only bind";
    model.remoteViewerDetail = health.lanBindDetail.empty()
        ? L"The local server is bound only to loopback, so other devices cannot reach it."
        : health.lanBindDetail;
    model.remoteViewerAction = L"Bind to 0.0.0.0 or the selected LAN IPv4 address, then restart the server.";
    return model;
  }

  if (MissingHostIp(session.hostIp)) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = L"Host IPv4 missing";
    model.remoteViewerDetail = L"No usable LAN IPv4 is currently selected for sharing, so the Viewer URL is not reliable yet.";
    model.remoteViewerAction = L"Refresh adapter detection or select the recommended active adapter before sharing the Viewer URL.";
    return model;
  }

  if (health.activeIpv4Candidates > 1 && !health.selectedIpRecommended) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = L"Adapter selection mismatch";
    model.remoteViewerDetail = health.adapterHint.empty()
        ? L"More than one active IPv4 adapter is present and the current share address does not match the recommended adapter."
        : health.adapterHint;
    model.remoteViewerAction = L"Switch to the recommended adapter IP or disable the unrelated adapter, then export/share again.";
    return model;
  }

  if (!health.hostIpReachable) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = model.firewallReady ? L"LAN self-probe failed" : L"LAN path blocked";
    model.remoteViewerDetail = health.hostIpReachableDetail.empty()
        ? L"The selected host LAN address did not answer the HTTP /health probe, so the viewer path is not reliable yet."
        : health.hostIpReachableDetail;
    model.remoteViewerAction = model.firewallReady
        ? L"Re-detect network selection and confirm the viewer is on the same subnet / hotspot before handing off the Viewer URL."
        : L"Open Windows Firewall settings or run the network diagnostics helper, then retry the Viewer URL from another device on the same LAN.";
    return model;
  }

  if (!model.firewallReady) {
    model.remoteViewerReady = false;
    model.remoteViewerLabel = L"Likely blocked by firewall policy";
    model.remoteViewerDetail = L"The host LAN address answered local probes, but Windows Firewall does not show a clear inbound allow path for viewer traffic.";
    model.remoteViewerAction = L"Create or enable an inbound allow rule for the server executable or the configured TCP port, then retry from another device.";
    return model;
  }

  model.remoteViewerReady = true;
  model.remoteViewerLabel = L"Viewer path looks ready";
  model.remoteViewerDetail = L"Local health, LAN bind, adapter selection, and firewall checks all look compatible with remote viewers on the same network.";
  model.remoteViewerAction = L"Keep the viewer on the same LAN / hotspot and open the Viewer URL directly in a browser.";
  return model;
}

} // namespace lan::runtime
