#include "core/runtime/share_artifact_service.h"

#include "core/runtime/network_diagnostics_policy.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <system_error>
#include <codecvt>

namespace lan::runtime {

namespace fs = std::filesystem;

std::wstring Utf8ToWide(std::string_view s) {
  if (s.empty()) return L"";
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
  return conv.from_bytes(s.data(), s.data() + s.size());
}

std::string WideToUtf8(std::wstring_view s) {
  if (s.empty()) return {};
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
  return conv.to_bytes(s.data(), s.data() + s.size());
}

bool IsHostStateServerRunning(std::wstring_view state) {
  return !state.empty() && state != L"stopped";
}

bool IsHostStateSharing(std::wstring_view state) {
  return state == L"sharing" || state == L"shared" || state == L"streaming";
}

bool IsHostStateReadyOrLoading(std::wstring_view state) {
  return state == L"ready" || state == L"loading" || IsHostStateSharing(state);
}

bool WriteUtf8File(const fs::path& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
  return f.good();
}

static std::string BuildShareCardHtml(std::wstring_view networkMode,
                                      std::wstring_view hostIp,
                                      int port,
                                      std::wstring_view room,
                                      std::wstring_view token,
                                      std::wstring_view hostState,
                                      std::size_t rooms,
                                      std::size_t viewers,
                                      std::wstring_view hotspotSsid,
                                      std::wstring_view hotspotPassword,
                                      bool hotspotRunning,
                                      bool wifiDirectApiAvailable,
                                      std::wstring_view hostUrl,
                                      std::wstring_view viewerUrl,
                                      std::string_view bundleJson) {
    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <meta name="theme-color" content="#050816"/>
  <title>LAN Screen Share Share Card</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
      background:
        radial-gradient(circle at 14% 12%, rgba(72,214,255,.22), transparent 24%),
        radial-gradient(circle at 84% 8%, rgba(156,183,255,.18), transparent 26%),
        linear-gradient(180deg, #050816 0%, #040507 100%);
      color: #f4fbff;
      position: relative;
      overflow-x: hidden;
    }
    body::before,
    body::after {
      content: "";
      position: fixed;
      border-radius: 999px;
      pointer-events: none;
      filter: blur(12px);
      opacity: .85;
    }
    body::before {
      top: 8vh;
      right: -10vw;
      width: 34vw;
      height: 34vw;
      background: radial-gradient(circle, rgba(72,214,255,.18) 0%, rgba(72,214,255,0) 72%);
    }
    body::after {
      left: -8vw;
      bottom: 10vh;
      width: 28vw;
      height: 28vw;
      background: radial-gradient(circle, rgba(156,183,255,.14) 0%, rgba(156,183,255,0) 72%);
    }
    .page { position: relative; z-index: 1; max-width: 1240px; margin: 0 auto; padding: 28px 24px 88px; }
    .grid { display: grid; grid-template-columns: minmax(0, 1.08fr) minmax(320px, 420px); gap: 20px; align-items: start; }
    .card {
      background: rgba(8,12,24,.76);
      border: 1px solid rgba(255,255,255,.1);
      border-radius: 28px;
      padding: 22px;
      box-shadow: 0 24px 68px rgba(0,0,0,.42);
      backdrop-filter: blur(22px);
    }
    h1 { margin: 0 0 14px; font-size: clamp(30px, 5vw, 48px); letter-spacing: -.04em; }
    .meta { display: grid; grid-template-columns: 136px 1fr; gap: 10px 14px; margin-bottom: 18px; }
    .meta div:nth-child(odd) { color: rgba(201,212,255,.62); letter-spacing: .12em; text-transform: uppercase; font-size: 11px; }
    .url, .mono {
      display: block;
      overflow-wrap: anywhere;
      background: rgba(4,9,19,.92);
      border: 1px solid rgba(255,255,255,.08);
      border-radius: 16px;
      padding: 14px;
      margin-top: 8px;
      color: #a9dbff;
      text-decoration: none;
      font-family: ui-monospace, SFMono-Regular, Consolas, monospace;
      box-shadow: inset 0 1px 0 rgba(255,255,255,.03);
    }
    .mono { color: #f4fbff; }
    .actions { display:flex; gap:10px; flex-wrap:wrap; margin-top:18px; }
    button {
      padding: 10px 14px;
      border-radius: 999px;
      border: 1px solid rgba(255,255,255,.12);
      background: rgba(11,20,34,.78);
      color: #fff;
      cursor: pointer;
      backdrop-filter: blur(16px);
      transition: background .18s ease, border-color .18s ease, transform .18s ease;
    }
    button:hover {
      background: rgba(18,30,52,.92);
      border-color: rgba(125,211,252,.24);
      transform: translateY(-1px);
    }
    .tip { color: rgba(201,212,255,.76); font-size: 13px; line-height: 1.6; }
    .badge {
      display:inline-flex;
      align-items:center;
      border-radius:999px;
      padding:7px 12px;
      font-size:12px;
      background: rgba(8,12,24,.72);
      color:#dbeafe;
      border:1px solid rgba(255,255,255,.1);
      backdrop-filter: blur(18px);
    }
    .poster { display:flex; flex-direction:column; gap:16px; }
    .poster-hero {
      border-radius: 24px;
      border: 1px solid rgba(125,211,252,.16);
      background:
        radial-gradient(circle at top, rgba(34,69,194,.18), transparent 42%),
        linear-gradient(160deg, rgba(10,18,36,.92), rgba(6,12,24,.84));
      padding: 22px;
    }
    .poster-title { font-size: 28px; font-weight: 700; margin: 0 0 10px; letter-spacing: -.03em; }
    .poster-url {
      font-family: ui-monospace, SFMono-Regular, Consolas, monospace;
      line-height: 1.55;
      font-size: 16px;
      background: rgba(4,9,19,.76);
      border: 1px solid rgba(255,255,255,.1);
      padding: 14px;
      border-radius: 18px;
      overflow-wrap:anywhere;
    }
    .qr-shell {
      display:flex;
      align-items:center;
      justify-content:center;
      padding:20px;
      background:#ffffff;
      border-radius:24px;
      min-height:340px;
      box-shadow: inset 0 0 0 1px rgba(11,20,34,.08);
    }
    .qr-shell svg { width:min(100%, 320px); height:auto; display:block; }
    .statusbar { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-bottom:16px; }
    .live { color:#67e8f9; }
    .warn { color:#fbbf24; }
    .accent { color:#7dd3fc; }
    ol { margin: 10px 0 0 18px; padding: 0; }
    li { margin: 8px 0; color: rgba(228,236,255,.86); }
    .toast {
      position: fixed;
      right: 20px;
      bottom: 20px;
      z-index: 4;
      max-width: min(420px, calc(100vw - 40px));
      padding: 14px 16px;
      border-radius: 18px;
      border: 1px solid rgba(255,255,255,.12);
      background: rgba(6,12,24,.92);
      color: #f4fbff;
      box-shadow: 0 22px 54px rgba(0,0,0,.38);
      backdrop-filter: blur(18px);
      opacity: 0;
      transform: translateY(12px);
      pointer-events: none;
      transition: opacity .18s ease, transform .18s ease;
    }
    .toast[data-visible="true"] { opacity: 1; transform: translateY(0); }
    .toast.ok { border-color: rgba(143,242,208,.24); color: #dffef1; }
    .toast.warn { border-color: rgba(255,225,140,.22); color: #fff0b8; }
    @media (max-width: 960px) {
      .page { padding: 18px 16px 80px; }
      .grid { grid-template-columns: 1fr; }
      .meta { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main class="page">
    <div class="grid">
      <section class="card">
        <div class="statusbar">
          <span class="badge">Offline share card + local QR SVG</span>
          <span class="badge live">Live status: auto refresh</span>
          <span class="tip">Last sync: <span id="lastSyncText">embedded snapshot</span></span>
        </div>
        <h1>LAN Screen Share</h1>
        <div class="meta">
          <div>Mode</div><div id="modeText"></div>
          <div>Host IPv4</div><div id="hostIpText"></div>
          <div>Port</div><div id="portText"></div>
          <div>Room</div><div id="roomText"></div>
          <div>Token</div><div id="tokenText"></div>
          <div>Host State</div><div id="hostStateText"></div>
          <div>Rooms / Viewers</div><div><span id="roomsText"></span> / <span id="viewersText"></span></div>
          <div>Hotspot</div><div id="hotspotStateText"></div>
          <div>Wi-Fi Direct API</div><div id="wifiDirectApiText"></div>
          <div>Bundle Time</div><div id="generatedAtText"></div>
        </div>

        <div class="tip">Host URL</div>
        <a id="hostUrlLink" class="url" href="#"></a>

        <div class="tip" style="margin-top: 14px;">Viewer URL</div>
        <a id="viewerUrlLink" class="url" href="#"></a>

        <div class="tip" style="margin-top: 14px;">Hotspot SSID</div>
        <div id="ssidText" class="mono"></div>

        <div class="tip" style="margin-top: 14px;">Hotspot Password</div>
        <div id="pwdText" class="mono"></div>

        <div class="actions">
          <button id="copyViewerBtn" type="button">Copy Viewer URL</button>
          <button id="openViewerBtn" type="button">Open Viewer URL</button>
          <button id="copySsidBtn" type="button">Copy SSID</button>
          <button id="copyPwdBtn" type="button">Copy Password</button>
          <button id="openWizardBtn" type="button">Open Share Wizard</button>
          <button id="openReadmeBtn" type="button">Open README</button>
          <button id="openDiagBtn" type="button">Open Diagnostics</button>
        </div>
      </section>

      <aside class="card poster">
        <div class="poster-hero">
          <div class="poster-title">Open on another device</div>
          <div class="tip">This page is fully local. The QR below is rendered from a local JavaScript bundle and its status is refreshed from <span class="mono">share_status.js</span>.</div>
          <div class="poster-url" id="posterUrl"></div>
          <div class="tip" style="margin-top:12px;">Viewer count: <span id="viewerBadgeText" class="accent">0</span></div>
        </div>

        <div id="qrMount" class="qr-shell" aria-live="polite">Loading local QR…</div>

        <div class="actions" style="margin-top:0;">
          <button id="downloadQrBtn" type="button">Download QR SVG</button>
          <button id="copyUrlBtn2" type="button">Copy Viewer URL</button>
        </div>

        <div>
          <div class="tip">How to connect</div>
          <ol>
            <li id="connectLine1"></li>
            <li>Scan the QR, or open a browser and enter the Viewer URL exactly as shown.</li>
            <li>No certificate trust step is required in plain HTTP mode. Focus on LAN reachability instead.</li>
            <li>If hosted-network control is unavailable on this device, use Windows Mobile Hotspot settings as the fallback path.</li>
          </ol>
        </div>
      </aside>
    </div>
  </main>
  <div id="toast" class="toast" aria-live="polite"></div>

  <script id="bundleJson" type="application/json">)HTML";
    html << bundleJson;
    html << R"HTML(</script>
  <script src="./www/assets/share_card_qr.bundle.js"></script>
  <script>
    let state = JSON.parse(document.getElementById('bundleJson').textContent);
    let currentViewerUrl = '';
    let lastRenderedVersion = '';
    let pollTimer = null;
    let toastTimer = null;

    function setText(id, text) {
      const el = document.getElementById(id);
      if (el) el.textContent = text == null ? '' : String(text);
    }

    function setLink(id, href) {
      const el = document.getElementById(id);
      if (!el) return;
      el.href = href || '#';
      el.textContent = href || '';
    }

    function notify(message, tone) {
      const toast = document.getElementById('toast');
      if (!toast) return;
      toast.textContent = String(message || '');
      toast.className = 'toast' + (tone ? ' ' + tone : '');
      toast.setAttribute('data-visible', 'true');
      if (toastTimer) window.clearTimeout(toastTimer);
      toastTimer = window.setTimeout(() => toast.setAttribute('data-visible', 'false'), 2200);
    }

    async function copyText(text, okLabel) {
      try {
        await navigator.clipboard.writeText(String(text || ''));
        notify(okLabel, 'ok');
      } catch (_) {
        notify('Copy failed. Please copy it manually.', 'warn');
      }
    }

    function currentStateVersion(bundle) {
      return [bundle.generatedAt || '', bundle.hostState || '', bundle.rooms || 0, bundle.viewers || 0, bundle.links && bundle.links.viewerUrl || ''].join('|');
    }

    function updateSyncLabel(source) {
      const now = new Date();
      const stamp = now.toLocaleTimeString();
      setText('lastSyncText', source + ' @ ' + stamp);
    }

    function renderQr(force) {
      const mount = document.getElementById('qrMount');
      if (!force && currentViewerUrl === (state.links && state.links.viewerUrl || '')) return;
      currentViewerUrl = state.links && state.links.viewerUrl || '';
      try {
        if (!window.LanShareQr) throw new Error('Local QR bundle is missing');
        mount.style.background = '#ffffff';
        mount.style.color = '#111827';
        window.LanShareQr.renderInto(mount, currentViewerUrl, { cellSize: 8, margin: 4 });
      } catch (err) {
        mount.style.background = '#161a22';
        mount.style.color = '#f2f3f5';
        mount.textContent = 'Local QR failed to render. Open the Viewer URL manually.\n' + String(err);
      }
    }

    function downloadQr() {
      try {
        const svg = window.LanShareQr.buildSvgMarkup(currentViewerUrl, { cellSize: 8, margin: 4 });
        const blob = new Blob([svg], { type: 'image/svg+xml;charset=utf-8' });
        const href = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = href;
        a.download = 'viewer_qr.svg';
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(href), 1000);
        notify('QR SVG downloaded', 'ok');
      } catch (err) {
        notify('QR export failed: ' + err, 'warn');
      }
    }

    function renderBundle(bundle, source) {
      if (!bundle || !bundle.links) return;
      state = bundle;
      const hotspot = bundle.hotspot || {};
      const wifiDirect = bundle.wifiDirect || {};
      const viewerUrl = bundle.links.viewerUrl || '';
      const hostUrl = bundle.links.hostUrl || '';
      setText('modeText', bundle.networkMode || 'unknown');
      setText('hostIpText', bundle.hostIp || '(not found)');
      setText('portText', bundle.port || '');
      setText('roomText', bundle.room || '');
      setText('tokenText', bundle.token || '');
      setText('hostStateText', bundle.hostState || 'unknown');
      setText('roomsText', bundle.rooms || 0);
      setText('viewersText', bundle.viewers || 0);
      setText('hotspotStateText', hotspot.running ? 'running' : 'stopped');
      setText('wifiDirectApiText', wifiDirect.apiAvailable ? 'available' : 'not detected');
      setText('generatedAtText', bundle.generatedAt || '');
      setLink('hostUrlLink', hostUrl);
      setLink('viewerUrlLink', viewerUrl);
      setText('posterUrl', viewerUrl || '(viewer url missing)');
      setText('ssidText', hotspot.ssid || '(not configured)');
      setText('pwdText', hotspot.password || '(not configured)');
      setText('viewerBadgeText', bundle.viewers || 0);
      setText('connectLine1', hotspot.running
        ? 'Connect the phone or another PC to the hotspot shown on the left.'
        : 'Connect the phone or another PC to the same LAN / Wi-Fi as the host.');
      updateSyncLabel(source || 'embedded snapshot');
      const version = currentStateVersion(bundle);
      const forceQr = currentViewerUrl !== viewerUrl || lastRenderedVersion !== version;
      lastRenderedVersion = version;
      renderQr(forceQr);
    }

    function loadLiveStatus() {
      const script = document.createElement('script');
      script.src = './share_status.js?ts=' + Date.now();
      script.async = true;
      script.onload = () => {
        if (window.__LAN_SHARE_STATUS__) {
          renderBundle(window.__LAN_SHARE_STATUS__, 'share_status.js');
        }
        script.remove();
      };
      script.onerror = () => {
        setText('lastSyncText', 'share_status.js unavailable');
        script.remove();
      };
      document.head.appendChild(script);
    }

    window.addEventListener('lan-share-status', (ev) => {
      if (ev && ev.detail) renderBundle(ev.detail, 'share_status.js event');
    });

    document.getElementById('copyViewerBtn').onclick = () => copyText((state.links && state.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('copyUrlBtn2').onclick = () => copyText((state.links && state.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('openViewerBtn').onclick = () => window.open((state.links && state.links.viewerUrl) || '', '_blank');
    document.getElementById('copySsidBtn').onclick = () => copyText((state.hotspot && state.hotspot.ssid) || '', 'SSID copied');
    document.getElementById('copyPwdBtn').onclick = () => copyText((state.hotspot && state.hotspot.password) || '', 'Password copied');
    document.getElementById('openWizardBtn').onclick = () => { window.location.href = './share_wizard.html'; };
    document.getElementById('openReadmeBtn').onclick = () => window.open('./share_readme.txt', '_blank');
    document.getElementById('openDiagBtn').onclick = () => window.open('./share_diagnostics.txt', '_blank');
    document.getElementById('downloadQrBtn').onclick = downloadQr;

    renderBundle(state, 'embedded snapshot');
    pollTimer = window.setInterval(loadLiveStatus, 2000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) loadLiveStatus(); });
    window.addEventListener('focus', loadLiveStatus);
  </script>
</body>
</html>)HTML";

    return html.str();
}

static std::string JsonEscapeUtf8(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (unsigned char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[7];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(c));
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
            break;
        }
    }
    return out;
}

static std::string NormalizeSelfCheckSeverity(std::string_view severity) {
    const std::string sev(severity);
    if (sev == "P0" || sev == "p0") return "P0";
    if (sev == "P1" || sev == "p1") return "P1";
    return "P2";
}

static std::string NormalizeSelfCheckCategory(std::string_view category) {
    const std::string cat(category);
    if (cat == "certificate") return "certificate";
    if (cat == "network") return "network";
    return "sharing";
}

static std::string SeverityBadgeLabel(std::string_view severity) {
    const std::string sev = NormalizeSelfCheckSeverity(severity);
    if (sev == "P0") return "P0 blocker";
    if (sev == "P1") return "P1 fix soon";
    return "P2 advisory";
}

static void TouchSelfCheckCounters(SelfCheckReport& report,
                                   std::string_view severity,
                                   std::string_view category) {
    const std::string sev = NormalizeSelfCheckSeverity(severity);
    const std::string cat = NormalizeSelfCheckCategory(category);
    if (sev == "P0") ++report.p0;
    else if (sev == "P1") ++report.p1;
    else ++report.p2;

    if (cat == "certificate") ++report.certificateCount;
    else if (cat == "network") ++report.networkCount;
    else ++report.sharingCount;
}

static void AddSelfCheckItem(SelfCheckReport& report,
                             std::string id,
                             std::string title,
                             bool ok,
                             std::string okDetail,
                             std::string warnDetail,
                             std::string severity,
                             std::string category) {
    SelfCheckItem item;
    item.id = std::move(id);
    item.title = std::move(title);
    item.status = ok ? "ok" : "attention";
    item.detail = ok ? std::move(okDetail) : std::move(warnDetail);
    item.severity = NormalizeSelfCheckSeverity(severity);
    item.category = NormalizeSelfCheckCategory(category);
    item.ok = ok;
    report.items.push_back(std::move(item));
    ++report.total;
    if (ok) {
        ++report.passed;
    } else {
        TouchSelfCheckCounters(report, report.items.back().severity, report.items.back().category);
    }
}

static void AddFailureHint(SelfCheckReport& report,
                           std::string title,
                           std::string detail,
                           std::string action,
                           std::string severity,
                           std::string category) {
    FailureHint hint;
    hint.title = std::move(title);
    hint.detail = std::move(detail);
    hint.action = std::move(action);
    hint.severity = NormalizeSelfCheckSeverity(severity);
    hint.category = NormalizeSelfCheckCategory(category);
    report.failures.push_back(std::move(hint));
}

SelfCheckReport BuildSelfCheckReport(std::wstring_view hostState,
                                            std::wstring_view hostIp,
                                            std::wstring_view viewerUrl,
                                            std::size_t viewers,
                                            bool serverProcessRunning,
                                            bool certReady,
                                            std::wstring_view certDetail,
                                            bool portReady,
                                            std::wstring_view portDetail,
                                            bool localHealthReady,
                                            std::wstring_view localHealthDetail,
                                            bool hostIpReachable,
                                            std::wstring_view hostIpReachableDetail,
                                            bool lanBindReady,
                                            std::wstring_view lanBindDetail,
                                            int activeIpv4Candidates,
                                            bool selectedIpRecommended,
                                            std::wstring_view adapterHint,
                                            bool embeddedHostReady,
                                            std::wstring_view embeddedHostStatus,
                                            bool firewallReady,
                                            std::wstring_view firewallDetail,
                                            bool wifiAdapterPresent,
                                            bool hotspotSupported,
                                            bool wifiDirectApiAvailable,
                                            bool hotspotRunning,
                                            bool liveReady) {
    SelfCheckReport report;

    const bool serverReady = serverProcessRunning || IsHostStateServerRunning(hostState);
    const bool hostReady = IsHostStateReadyOrLoading(hostState);
    const bool hostSharing = IsHostStateSharing(hostState);
    const bool networkReady = !hostIp.empty() && hostIp != L"(not found)" && hostIp != L"0.0.0.0";
    const bool viewerReady = !viewerUrl.empty();
    const bool hotspotControlReady = hotspotSupported || hotspotRunning;
    const std::string portDetailUtf8 = WideToUtf8(std::wstring(portDetail));
    const std::string localHealthDetailUtf8 = WideToUtf8(std::wstring(localHealthDetail));
    const std::string hostIpReachableDetailUtf8 = WideToUtf8(std::wstring(hostIpReachableDetail));
    const std::string lanBindDetailUtf8 = WideToUtf8(std::wstring(lanBindDetail));
    const std::string adapterHintUtf8 = WideToUtf8(std::wstring(adapterHint));
    const std::string embeddedHostStatusUtf8 = WideToUtf8(std::wstring(embeddedHostStatus));
    const std::string certDetailUtf8 = WideToUtf8(std::wstring(certDetail));
    const std::string firewallDetailUtf8 = WideToUtf8(std::wstring(firewallDetail));

    RuntimeSessionState sessionState;
    sessionState.hostIp = std::wstring(hostIp);
    sessionState.hostPageState = std::wstring(hostState);
    sessionState.hotspotRunning = hotspotRunning;
    sessionState.hotspotSupported = hotspotSupported;
    sessionState.wifiAdapterPresent = wifiAdapterPresent;
    sessionState.wifiDirectApiAvailable = wifiDirectApiAvailable;

    RuntimeHealthState healthState;
    healthState.serverProcessRunning = serverProcessRunning;
    healthState.certReady = certReady;
    healthState.certDetail = std::wstring(certDetail);
    healthState.portReady = portReady;
    healthState.portDetail = std::wstring(portDetail);
    healthState.localHealthReady = localHealthReady;
    healthState.localHealthDetail = std::wstring(localHealthDetail);
    healthState.hostIpReachable = hostIpReachable;
    healthState.hostIpReachableDetail = std::wstring(hostIpReachableDetail);
    healthState.lanBindReady = lanBindReady;
    healthState.lanBindDetail = std::wstring(lanBindDetail);
    healthState.activeIpv4Candidates = static_cast<std::size_t>(std::max(activeIpv4Candidates, 0));
    healthState.selectedIpRecommended = selectedIpRecommended;
    healthState.adapterHint = std::wstring(adapterHint);
    healthState.embeddedHostReady = embeddedHostReady;
    healthState.embeddedHostStatus = std::wstring(embeddedHostStatus);
    healthState.firewallReady = firewallReady;
    healthState.firewallDetail = std::wstring(firewallDetail);
    const auto networkDiagnostics = BuildNetworkDiagnosticsViewModel(sessionState, healthState);

    AddSelfCheckItem(report,
                     "server-reachability",
                     "Server reachability",
                     serverReady,
                     "The desktop host reports the HTTP/WS service as running.",
                     "The desktop host is not reporting a running server. Press Start and wait for the host page to load.",
                     "P0",
                     "sharing");
    AddSelfCheckItem(report,
                     "listen-port",
                     "Listen port",
                     portReady,
                     portDetailUtf8.empty() ? "The configured TCP port looks available." : portDetailUtf8,
                     portDetailUtf8.empty()
                         ? "The configured TCP port is busy or invalid. Pick a free port before starting the host."
                         : portDetailUtf8,
                     "P0",
                     "network");
    AddSelfCheckItem(report,
                     "server-health-endpoint",
                     "Server health endpoint",
                     localHealthReady,
                     localHealthDetailUtf8.empty() ? "Local /health responded correctly." : localHealthDetailUtf8,
                     localHealthDetailUtf8.empty()
                         ? "The local HTTP/WS service did not respond to /health. Restart the service and re-run checks."
                         : localHealthDetailUtf8,
                     "P0",
                     "sharing");
    AddSelfCheckItem(report,
                     "lan-bind-address",
                     "LAN bind address",
                     lanBindReady,
                     lanBindDetailUtf8.empty() ? "Server bind address allows LAN clients." : lanBindDetailUtf8,
                     lanBindDetailUtf8.empty()
                         ? "Server bind address is loopback-only or invalid for LAN sharing."
                         : lanBindDetailUtf8,
                     "P0",
                     "network");
    AddSelfCheckItem(report,
                     "lan-entry-endpoint",
                     "LAN entry endpoint",
                     hostIpReachable,
                     hostIpReachableDetailUtf8.empty() ? "Selected host IP responded to /health." : hostIpReachableDetailUtf8,
                     hostIpReachableDetailUtf8.empty()
                         ? "The selected host IP did not answer the local /health probe."
                         : hostIpReachableDetailUtf8,
                     "P1",
                     "network");
    AddSelfCheckItem(report,
                     "firewall-inbound-path",
                     "Firewall inbound path",
                     networkDiagnostics.firewallReady,
                     firewallDetailUtf8.empty() ? "An inbound firewall allow path was detected for viewer traffic." : firewallDetailUtf8,
                     firewallDetailUtf8.empty()
                         ? "Windows Firewall does not yet show an inbound allow path for viewer traffic."
                         : firewallDetailUtf8,
                     networkDiagnostics.firewallReady ? "P2" : "P1",
                     "network");
    AddSelfCheckItem(report,
                     "remote-viewer-reachability",
                     "Remote viewer reachability",
                     networkDiagnostics.remoteViewerReady,
                     WideToUtf8(networkDiagnostics.remoteViewerDetail),
                     WideToUtf8(networkDiagnostics.remoteViewerDetail),
                     networkDiagnostics.remoteViewerReady ? "P2" : "P1",
                     "network");
    AddSelfCheckItem(report,
                     "adapter-selection",
                     "Adapter selection",
                     activeIpv4Candidates <= 1 || selectedIpRecommended,
                     adapterHintUtf8.empty() ? "The selected host IP matches the best active adapter." : adapterHintUtf8,
                     adapterHintUtf8.empty()
                         ? "Multiple adapters are active and the selected host IP may not be the best LAN target."
                         : adapterHintUtf8,
                     activeIpv4Candidates > 1 ? "P1" : "P2",
                     "network");
    AddSelfCheckItem(report,
                     "embedded-host-runtime",
                     "Embedded host runtime",
                     embeddedHostReady,
                     "Embedded host view is ready inside the desktop app.",
                     embeddedHostStatusUtf8 == "sdk-unavailable"
                         ? "WebView2 SDK support was not compiled into this build. Restore the desktop NuGet packages and rebuild, or use Open Host Page in an external browser."
                         : embeddedHostStatusUtf8 == "runtime-unavailable" || embeddedHostStatusUtf8 == "controller-unavailable"
                               ? "WebView2 runtime did not initialize. The host page can still be opened in an external browser."
                               : embeddedHostStatusUtf8 == "initializing"
                                     ? "Embedded host view is still initializing. Wait a moment, then re-run checks."
                                     : "Embedded host view is unavailable. Use Open Host Page in an external browser as fallback.",
                     "P1",
                     "sharing");
    AddSelfCheckItem(report,
                     "host-sharing-state",
                     "Host sharing state",
                     hostSharing,
                     "Screen sharing is active in the embedded host page.",
                     !embeddedHostReady
                         ? "The embedded host page is unavailable inside the desktop app. Use Open Host Page in an external browser and start sharing there."
                         : hostReady
                         ? "The host page is loaded, but screen sharing does not look active yet. Click Start Share inside the host page."
                         : "The host page is not ready yet. Reopen the host page or restart the local server.",
                     "P1",
                     "sharing");
    AddSelfCheckItem(report,
                     "host-network",
                     "Host network",
                     networkReady,
                     std::string("Host IPv4 looks usable: ") + WideToUtf8(std::wstring(hostIp)),
                     "Host IPv4 is missing. Refresh the IP, or start hotspot / join a LAN before exporting again.",
                     "P0",
                     "network");
    AddSelfCheckItem(report,
                     "viewer-entry-url",
                     "Viewer entry URL",
                     viewerReady,
                     "Viewer URL and QR target are present in the bundle.",
                     "Viewer URL is empty. Re-export after host IP and port are valid.",
                     "P1",
                     "sharing");
    AddSelfCheckItem(report,
                     "transport-mode",
                     "Transport mode",
                     certReady,
                     certDetailUtf8.empty()
                         ? "Plain HTTP mode is active for the local admin and sharing flow."
                         : certDetailUtf8,
                     certDetailUtf8.empty()
                         ? "The local transport mode is not ready yet."
                         : certDetailUtf8,
                     "P0",
                     "certificate");
    AddSelfCheckItem(report,
                     "wifi-adapter",
                     "Wi-Fi adapter",
                     wifiAdapterPresent,
                     "A Wi-Fi adapter is present on this machine.",
                     "No Wi-Fi adapter was detected. LAN sharing can still work, but hotspot and Wi-Fi Direct paths will be limited.",
                     "P2",
                     "network");
    AddSelfCheckItem(report,
                     "hotspot-control",
                     "Hotspot control",
                     hotspotControlReady,
                     hotspotRunning
                         ? "Hotspot support is active and the hotspot is currently running."
                         : "Hotspot control is available on this machine (best effort).",
                     wifiDirectApiAvailable
                         ? "Hosted-network control was not detected. Use Windows Mobile Hotspot settings or Wi-Fi Direct pairing as the fallback path."
                         : "Hosted-network control was not detected. Use Windows Mobile Hotspot settings as the fallback path.",
                     "P2",
                     "network");
    AddSelfCheckItem(report,
                     "live-bundle-refresh",
                     "Live bundle refresh",
                     liveReady,
                     "share_status.js is configured, so already-open pages can refresh in place.",
                     "Live status script is not configured; reopen the exported bundle after regenerating it.",
                     "P2",
                     "sharing");

    if (!serverReady) {
        AddFailureHint(report,
            "Server not running",
            "Press Start in the desktop host and wait for the host page to move out of the stopped state before asking a viewer to connect.",
            "Press Start in the desktop host, confirm Status becomes running, then let the embedded host page finish loading.",
            "P0",
            "sharing");
    }
    if (!portReady && !serverProcessRunning) {
        AddFailureHint(report,
            "Listen port unavailable",
            portDetailUtf8.empty()
                ? "The configured TCP port is busy or invalid, so the local HTTP/WS service cannot start on it."
                : portDetailUtf8,
            "Pick a different port or stop the process that already owns this port, then press Start again.",
            "P0",
            "network");
    }
    if (serverProcessRunning && !localHealthReady) {
        AddFailureHint(report,
            "Local health check failed",
            localHealthDetailUtf8.empty()
                ? "The server process started, but the local /health endpoint did not return a healthy response."
                : localHealthDetailUtf8,
            "Restart the local server. If the problem persists, inspect the server log and cert/www output beside the executable.",
            "P0",
            "sharing");
    }
    if (!lanBindReady) {
        AddFailureHint(report,
            "Server bind is not LAN-reachable",
            lanBindDetailUtf8.empty()
                ? "The desktop host is configured to bind only to loopback, so other devices cannot connect."
                : lanBindDetailUtf8,
            "Change the bind address to 0.0.0.0 or the selected LAN IP, then restart the server.",
            "P0",
            "network");
    }
    if (serverProcessRunning && networkReady && !hostIpReachable) {
        AddFailureHint(report,
            "Selected LAN address is not reachable",
            hostIpReachableDetailUtf8.empty()
                ? "The selected host IP did not answer the /health probe, so the exported Viewer URL is likely wrong for this machine."
                : hostIpReachableDetailUtf8,
            "Refresh Host IPv4, prefer the recommended active adapter, then restart the server or re-export the bundle.",
            "P1",
            "network");
    }
    if (!networkReady) {
        AddFailureHint(report,
            "Host IPv4 missing",
            "Refresh the detected IP or move the host onto a real LAN / hotspot before exporting the bundle again.",
            "Press Refresh beside Host IPv4. If it is still missing, connect the host to LAN or start the hotspot, then re-run checks.",
            "P0",
            "network");
    }
    if (!networkDiagnostics.firewallReady && serverProcessRunning) {
        AddFailureHint(report,
            "Inbound firewall path needs attention",
            firewallDetailUtf8.empty()
                ? "Windows Firewall does not currently show an enabled inbound allow rule for the viewer path."
                : firewallDetailUtf8,
            WideToUtf8(networkDiagnostics.firewallAction),
            "P1",
            "network");
    }
    if (!networkDiagnostics.remoteViewerReady && serverProcessRunning) {
        AddFailureHint(report,
            "Remote viewer path is not fully ready",
            WideToUtf8(networkDiagnostics.remoteViewerDetail),
            WideToUtf8(networkDiagnostics.remoteViewerAction),
            hostIpReachable ? "P1" : "P0",
            "network");
    }
    if (!certReady) {
        AddFailureHint(report,
            "Transport mode is not ready",
            certDetailUtf8.empty()
                ? "The local transport mode is not fully initialized for this session."
                : certDetailUtf8,
            "Restart the local server and confirm the local admin shell loads before exporting or sharing again.",
            "P0",
            "certificate");
    }
    if (serverReady && !hostSharing && embeddedHostReady) {
        AddFailureHint(report,
            "Host page not sharing yet",
            "Open the embedded host page and complete the screen picker. Viewers can connect only after the host page starts media capture.",
            "In the embedded host page, click Start Share and finish the screen picker before sharing the Viewer URL.",
            "P1",
            "sharing");
    }
    if (!embeddedHostReady) {
        AddFailureHint(report,
            "Embedded host unavailable",
            embeddedHostStatusUtf8.empty()
                ? "The embedded host runtime is unavailable inside the desktop app."
                : "Embedded host runtime status: " + embeddedHostStatusUtf8 + ".",
            "Use Open Host Page to start sharing in an external browser, or install/repair the WebView2 runtime on this machine.",
            "P1",
            "sharing");
    }
    if (activeIpv4Candidates > 1 && !selectedIpRecommended) {
        AddFailureHint(report,
            "Multiple adapters are active",
            adapterHintUtf8.empty()
                ? "This machine has more than one active IPv4 adapter, so the exported share IP may not match the intended LAN."
                : adapterHintUtf8,
            "Use the recommended adapter IP for sharing, or disable the unrelated adapter before exporting diagnostics again.",
            "P1",
            "network");
    }
    if (viewerReady && !hotspotRunning && viewers == 0) {
        AddFailureHint(report,
            "Viewer may be on the wrong network",
            "Make sure the other device is on the same LAN / Wi-Fi. If not, start the hotspot or complete Wi-Fi Direct pairing first.",
            "Move the viewer onto the same LAN or join the hotspot shown in Share Info, then reopen the Viewer URL.",
            "P1",
            "network");
    }
    if (!wifiAdapterPresent) {
        AddFailureHint(report,
            "No Wi-Fi adapter detected",
            "Hotspot and Wi-Fi Direct flows may not work on this machine. Keep both devices on wired LAN / same router Wi-Fi instead.",
            "Prefer an existing wired LAN or the same router Wi-Fi for both devices; do not rely on hotspot on this machine.",
            "P2",
            "network");
    }
    if (!hotspotControlReady && !wifiDirectApiAvailable) {
        AddFailureHint(report,
            "No local wireless fallback detected",
            "Hosted-network control and Wi-Fi Direct API both look unavailable. Use an existing LAN/Wi-Fi or open Windows Mobile Hotspot settings for manual setup.",
            "Open Windows Mobile Hotspot settings for manual setup, or keep both devices on the same existing LAN/Wi-Fi.",
            "P2",
            "network");
    }

    return report;
}

static int SelfCheckSeveritySortKey(std::string_view severity) {
    const std::string sev = NormalizeSelfCheckSeverity(severity);
    if (sev == "P0") return 0;
    if (sev == "P1") return 1;
    return 2;
}

static std::vector<FailureHint> BuildOperatorFirstActions(const SelfCheckReport& report,
                                                          std::size_t maxCount = 3) {
    std::vector<FailureHint> actions = report.failures;
    std::stable_sort(actions.begin(), actions.end(), [](const FailureHint& a, const FailureHint& b) {
        const int ak = SelfCheckSeveritySortKey(a.severity);
        const int bk = SelfCheckSeveritySortKey(b.severity);
        if (ak != bk) return ak < bk;
        if (a.category != b.category) return a.category < b.category;
        return a.title < b.title;
    });
    if (actions.size() > maxCount) {
        actions.resize(maxCount);
    }
    return actions;
}

static std::wstring BuildMainDiagnosticSummaryText(const SelfCheckReport& report) {
    std::wstringstream ss;
    ss << L"Overall: " << report.passed << L" / " << report.total << L" checks passed\r\n";
    ss << L"Severity: P0 " << report.p0 << L" / P1 " << report.p1 << L" / P2 " << report.p2 << L"\r\n";
    ss << L"Categories: transport " << report.certificateCount << L" / net " << report.networkCount << L" / sharing " << report.sharingCount << L"\r\n";
    if (!report.failures.empty()) {
        const auto& top = report.failures.front();
        ss << L"Top issue: [" << Utf8ToWide(top.severity) << L"][" << Utf8ToWide(top.category)
           << L"] " << Utf8ToWide(top.title) << L"\r\n";
    } else {
        ss << L"Top issue: none\r\n";
    }
    if (report.p0 > 0) {
        ss << L"Decision: fix P0 items before sending the bundle to viewers.";
    } else if (report.p1 > 0) {
        ss << L"Decision: bundle is usable, but clear the P1 items for a smoother handoff.";
    } else {
        ss << L"Decision: no blocker detected. You can share the current bundle.";
    }
    return ss.str();
}

static std::wstring BuildOperatorFirstActionsText(const SelfCheckReport& report) {
    const auto actions = BuildOperatorFirstActions(report, 3);
    std::wstringstream ss;
    if (actions.empty()) {
        ss << L"No urgent first action right now.\r\n";
        ss << L"1. Share the current bundle if the viewer is ready.\r\n";
        ss << L"2. Use Re-run Checks after any state change (server, IP, hotspot, sharing).";
        return ss.str();
    }

    for (std::size_t i = 0; i < actions.size(); ++i) {
        const auto& action = actions[i];
        ss << (i + 1) << L". [" << Utf8ToWide(action.severity) << L"][" << Utf8ToWide(action.category) << L"] "
           << Utf8ToWide(action.title) << L"\r\n   "
           << Utf8ToWide(action.action.empty() ? action.detail : action.action);
        if (i + 1 < actions.size()) ss << L"\r\n\r\n";
    }
    return ss.str();
}

SelfCheckReport BuildSelfCheckReport(const RuntimeSessionState& session,
                                     const RuntimeHealthState& health,
                                     bool liveReady) {
    return BuildSelfCheckReport(session.hostPageState,
                                session.hostIp,
                                BuildViewerUrl(session),
                                session.lastViewers,
                                health.serverProcessRunning,
                                health.certReady,
                                health.certDetail,
                                health.portReady,
                                health.portDetail,
                                health.localHealthReady,
                                health.localHealthDetail,
                                health.hostIpReachable,
                                health.hostIpReachableDetail,
                                health.lanBindReady,
                                health.lanBindDetail,
                                static_cast<int>(health.activeIpv4Candidates),
                                health.selectedIpRecommended,
                                health.adapterHint,
                                health.embeddedHostReady,
                                health.embeddedHostStatus,
                                health.firewallReady,
                                health.firewallDetail,
                                session.wifiAdapterPresent,
                                session.hotspotSupported,
                                session.wifiDirectApiAvailable,
                                session.hotspotRunning,
                                liveReady);
}

std::wstring BuildSelfCheckSummaryLine(const SelfCheckReport& report) {
    std::wstringstream ss;
    ss << report.passed << L" / " << report.total << L" ok";
    if (report.p0 > 0 || report.p1 > 0 || report.p2 > 0) {
        ss << L" | P0 " << report.p0 << L" / P1 " << report.p1 << L" / P2 " << report.p2;
    }
    if (!report.failures.empty()) {
        ss << L" | " << Utf8ToWide(report.failures.front().title);
    }
    return ss.str();
}

static std::string BuildSelfCheckJson(const SelfCheckReport& report) {
    std::ostringstream json;
    json << "{\n";
    json << "    \"passed\": " << report.passed << ",\n";
    json << "    \"total\": " << report.total << ",\n";
    json << "    \"severityCounts\": {\n";
    json << "      \"P0\": " << report.p0 << ",\n";
    json << "      \"P1\": " << report.p1 << ",\n";
    json << "      \"P2\": " << report.p2 << "\n";
    json << "    },\n";
    json << "    \"categoryCounts\": {\n";
    json << "      \"certificate\": " << report.certificateCount << ",\n";
    json << "      \"network\": " << report.networkCount << ",\n";
    json << "      \"sharing\": " << report.sharingCount << "\n";
    json << "    },\n";
    json << "    \"items\": [\n";
    for (size_t i = 0; i < report.items.size(); ++i) {
        const auto& item = report.items[i];
        json << "      {\n";
        json << "        \"id\": \"" << JsonEscapeUtf8(item.id) << "\",\n";
        json << "        \"title\": \"" << JsonEscapeUtf8(item.title) << "\",\n";
        json << "        \"status\": \"" << JsonEscapeUtf8(item.status) << "\",\n";
        json << "        \"severity\": \"" << JsonEscapeUtf8(item.severity) << "\",\n";
        json << "        \"category\": \"" << JsonEscapeUtf8(item.category) << "\",\n";
        json << "        \"ok\": " << (item.ok ? "true" : "false") << ",\n";
        json << "        \"detail\": \"" << JsonEscapeUtf8(item.detail) << "\"\n";
        json << "      }" << (i + 1 < report.items.size() ? "," : "") << "\n";
    }
    json << "    ],\n";
    json << "    \"issues\": [\n";
    for (size_t i = 0; i < report.failures.size(); ++i) {
        const auto& item = report.failures[i];
        json << "      {\n";
        json << "        \"title\": \"" << JsonEscapeUtf8(item.title) << "\",\n";
        json << "        \"severity\": \"" << JsonEscapeUtf8(item.severity) << "\",\n";
        json << "        \"category\": \"" << JsonEscapeUtf8(item.category) << "\",\n";
        json << "        \"action\": \"" << JsonEscapeUtf8(item.action) << "\",\n";
        json << "        \"detail\": \"" << JsonEscapeUtf8(item.detail) << "\"\n";
        json << "      }" << (i + 1 < report.failures.size() ? "," : "") << "\n";
    }
    json << "    ]\n";
    json << "  }";
    return json.str();
}

static std::string BuildDesktopSelfCheckHtml(std::string_view bundleJson) {
    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>LAN Screen Share - Desktop Self-Check</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body { margin:0; font-family: system-ui, -apple-system, "Segoe UI", sans-serif; background:#0b1020; color:#edf2f7; }
    .page { max-width: 1180px; margin:0 auto; padding:24px; }
    .hero { display:grid; grid-template-columns:1.15fr .85fr; gap:18px; margin-bottom:20px; }
    .card { background:#111827; border:1px solid #23304a; border-radius:18px; padding:20px; box-shadow:0 10px 30px rgba(0,0,0,.24); }
    .pill { display:inline-flex; align-items:center; gap:8px; border-radius:999px; padding:6px 10px; background:#18243b; border:1px solid #30415f; color:#dbeafe; font-size:12px; }
    .pill.ok { background:#0f2b1f; border-color:#1f6f43; color:#d1fae5; }
    .pill.warn { background:#2d1e08; border-color:#7c5d1a; color:#fde68a; }
    .pill.p0 { background:#35121a; border-color:#9f1239; color:#fecdd3; }
    .pill.p1 { background:#312113; border-color:#b45309; color:#fde68a; }
    .pill.p2 { background:#15253c; border-color:#2563eb; color:#bfdbfe; }
    .pill.certificate { background:#1f1835; border-color:#7c3aed; color:#ddd6fe; }
    .pill.network { background:#0f2530; border-color:#0891b2; color:#bae6fd; }
    .pill.sharing { background:#172436; border-color:#4f46e5; color:#c7d2fe; }
    .sub { color:#9fb0c8; line-height:1.6; }
    .kv { display:grid; grid-template-columns:140px 1fr; gap:8px 12px; margin-top:14px; }
    .kv div:nth-child(odd) { color:#9fb0c8; }
    .mono { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; overflow-wrap:anywhere; }
    .checks { display:grid; grid-template-columns:repeat(2, minmax(0,1fr)); gap:12px; }
    .check { border:1px solid #263656; border-radius:14px; padding:14px; background:#0d172b; }
    .check.ok { border-color:#1f6f43; background:#0d2218; }
    .check.warn { border-color:#7c5d1a; background:#231a09; }
    .issues { margin:0; padding-left:18px; }
    .issues li { margin:10px 0; }
    .actions, .filter-row, .summary-row { display:flex; flex-wrap:wrap; gap:10px; margin-top:16px; align-items:center; }
    button, a.btn { appearance:none; text-decoration:none; border:1px solid #334766; background:#16233a; color:#fff; padding:10px 14px; border-radius:12px; cursor:pointer; display:inline-flex; align-items:center; gap:8px; }
    button.filter { padding:8px 12px; font-size:12px; border-radius:999px; background:#10192b; }
    button.filter.active { background:#244064; border-color:#7dd3fc; color:#e0f2fe; }
    .muted { opacity:.78; }
    .hidden { display:none !important; }
    .check-head { display:flex; justify-content:space-between; gap:12px; align-items:flex-start; margin-bottom:10px; }
    .check-meta { display:flex; flex-wrap:wrap; gap:8px; justify-content:flex-end; }
    .section-title { margin:0 0 10px; }
    .small-gap { margin-top:8px; }
    @media (max-width: 920px) { .hero, .checks { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <main class="page">
    <section class="hero">
      <div class="card">
        <div class="summary-row">
          <span class="pill">Desktop self-check</span>
          <span class="pill" id="summaryPill">Running…</span>
          <span class="sub">Last sync: <span id="lastSyncText">embedded snapshot</span></span>
        </div>
        <h1 style="margin:12px 0;">Desktop self-check report</h1>
        <div class="sub">This page uses the same exported bundle data as Share Wizard, but focuses on host-side troubleshooting from the desktop operator view. The checks below now carry a P0 / P1 / P2 severity and a transport / network / sharing category.</div>
        <div class="kv">
          <div>Host state</div><div id="hostStateText"></div>
          <div>Host IPv4</div><div id="hostIpText"></div>
          <div>Room / viewers</div><div><span id="roomText"></span> / <span id="viewersText"></span></div>
          <div>Viewer URL</div><div id="viewerUrlText" class="mono"></div>
          <div>Hotspot</div><div id="hotspotText"></div>
          <div>Transport</div><div id="certText"></div>
        </div>
        <div class="summary-row">
          <span class="pill p0" id="p0Pill">P0: 0</span>
          <span class="pill p1" id="p1Pill">P1: 0</span>
          <span class="pill p2" id="p2Pill">P2: 0</span>
          <span class="pill certificate" id="certPill">Transport: 0</span>
          <span class="pill network" id="networkPill">Network: 0</span>
          <span class="pill sharing" id="sharingPill">Sharing: 0</span>
        </div>
        <div class="actions">
          <button id="openShareWizardBtn" type="button">Open Share Wizard</button>
          <button id="openShareCardBtn" type="button">Open Share Card</button>
          <a class="btn" href="./share_diagnostics.txt" target="_blank" rel="noopener">Open Diagnostics TXT</a>
          <button id="copyViewerBtn" type="button">Copy Viewer URL</button>
        </div>
      </div>
      <div class="card">
        <h2 class="section-title">Self-check summary</h2>
        <div class="sub">Passed: <span id="summaryText">0 / 0</span></div>
        <div class="small-gap sub" id="filterSummaryText">Showing all checks</div>
        <ul id="issueList" class="issues"></ul>
        <h2 class="section-title" style="margin-top:18px;">Suggested next actions</h2>
        <div class="sub">These now consume the exported <span class="mono">selfCheck.issues[].action</span> field first, so the desktop report, Share Wizard, and text diagnostics stay aligned.</div>
        <ul id="actionList" class="issues"></ul>
      </div>
    </section>

    <section class="card" style="margin-bottom:20px;">
      <h2 class="section-title">Filter view</h2>
      <div class="sub">Use these to focus on one problem class or one severity tier when you are troubleshooting with someone remotely.</div>
      <div class="filter-row">
        <strong>Category</strong>
        <button class="filter active" type="button" data-filter-kind="category" data-filter-value="all">All</button>
        <button class="filter" type="button" data-filter-kind="category" data-filter-value="certificate">Transport</button>
        <button class="filter" type="button" data-filter-kind="category" data-filter-value="network">Network</button>
        <button class="filter" type="button" data-filter-kind="category" data-filter-value="sharing">Sharing</button>
      </div>
      <div class="filter-row">
        <strong>Severity</strong>
        <button class="filter active" type="button" data-filter-kind="severity" data-filter-value="all">All</button>
        <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P0">P0</button>
        <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P1">P1</button>
        <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P2">P2</button>
      </div>
    </section>

    <section class="card">
      <h2 class="section-title">Checks</h2>
      <div id="checkGrid" class="checks"></div>
    </section>
  </main>

  <script id="bundleJson" type="application/json">)HTML";
    html << bundleJson;
    html << R"HTML(</script>
  <script>
    let bundle = JSON.parse(document.getElementById('bundleJson').textContent);
    const activeFilters = { category: 'all', severity: 'all' };

    function setText(id, text) {
      const el = document.getElementById(id);
      if (el) el.textContent = text == null ? '' : String(text);
    }

    function htmlEscape(v) {
      return String(v == null ? '' : v)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;')
        .replaceAll("'", '&#39;');
    }

    function normalizeSeverity(v) {
      return v === 'P0' || v === 'P1' ? v : 'P2';
    }

    function normalizeCategory(v) {
      return v === 'certificate' || v === 'network' ? v : 'sharing';
    }

    function severityClass(v) {
      return normalizeSeverity(v).toLowerCase();
    }

    function categoryClass(v) {
      return normalizeCategory(v);
    }

    async function copyViewerUrl() {
      try {
        await navigator.clipboard.writeText((bundle.links && bundle.links.viewerUrl) || '');
        alert('Viewer URL copied');
      } catch (_) {
        alert('Copy failed. Please copy it manually.');
      }
    }

    function updateSyncLabel(source) {
      const now = new Date();
      setText('lastSyncText', source + ' @ ' + now.toLocaleTimeString());
    }

    function updateSummaryPills(report) {
      const sev = report.severityCounts || {};
      const cat = report.categoryCounts || {};
      setText('p0Pill', 'P0: ' + (sev.P0 || 0));
      setText('p1Pill', 'P1: ' + (sev.P1 || 0));
      setText('p2Pill', 'P2: ' + (sev.P2 || 0));
      setText('certPill', 'Transport: ' + (cat.certificate || 0));
      setText('networkPill', 'Network: ' + (cat.network || 0));
      setText('sharingPill', 'Sharing: ' + (cat.sharing || 0));
    }

    function filterItems(items) {
      return (items || []).filter(item => {
        const categoryOk = activeFilters.category === 'all' || normalizeCategory(item.category) === activeFilters.category;
        const severityOk = activeFilters.severity === 'all' || normalizeSeverity(item.severity) === activeFilters.severity;
        return categoryOk && severityOk;
      });
    }
    function sortIssuesByPriority(items) {
      return (items || []).slice().sort((a, b) => {
        const sevOrder = { P0: 0, P1: 1, P2: 2 };
        const ak = sevOrder[normalizeSeverity(a.severity)] ?? 9;
        const bk = sevOrder[normalizeSeverity(b.severity)] ?? 9;
        if (ak !== bk) return ak - bk;
        const ac = normalizeCategory(a.category);
        const bc = normalizeCategory(b.category);
        if (ac !== bc) return ac.localeCompare(bc);
        return String(a.title || '').localeCompare(String(b.title || ''));
      });
    }

    function normalizeIssues(issues) {
      return sortIssuesByPriority((issues || []).map(item => ({
        title: item.title,
        detail: item.detail,
        action: item.action,
        category: normalizeCategory(item.category),
        severity: normalizeSeverity(item.severity)
      })));
    }

    function renderIssues(issues) {
      const root = document.getElementById('issueList');
      if (!root) return;
      const filtered = filterItems(normalizeIssues(issues));
      if (!filtered.length) {
        root.innerHTML = '<li><strong>No matching issue in this filter view.</strong> Switch back to All to review the full troubleshooting list.</li>';
        return;
      }
      root.innerHTML = filtered.map(item => {
        const actionLine = item.action ? '<div class="small-gap muted"><strong>Action:</strong> ' + htmlEscape(item.action) + '</div>' : '';
        return '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(item.detail) +
          actionLine +
          ' <span class="pill ' + severityClass(item.severity) + '">' + htmlEscape(item.severity) + '</span>' +
          ' <span class="pill ' + categoryClass(item.category) + '">' + htmlEscape(item.category) + '</span></li>';
      }).join('');
    }

    function renderActionItems(issues) {
      const root = document.getElementById('actionList');
      if (!root) return;
      const filtered = filterItems(normalizeIssues(issues)).filter(item => !!(item.action || item.detail));
      if (!filtered.length) {
        root.innerHTML = '<li><strong>No urgent action needed.</strong> The exported issue list does not currently contain a blocking action. You can share the current bundle or use Re-run Checks after any state change.</li>';
        return;
      }
      root.innerHTML = filtered.map(item =>
        '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(item.action || item.detail) +
        ' <span class="pill ' + severityClass(item.severity) + '">' + htmlEscape(item.severity) + '</span>' +
        ' <span class="pill ' + categoryClass(item.category) + '">' + htmlEscape(item.category) + '</span></li>').join('');
    }

    function renderChecks(items) {
      const root = document.getElementById('checkGrid');
      if (!root) return;
      const filtered = filterItems(items || []);
      if (!filtered.length) {
        root.innerHTML = '<div class="check warn"><div class="check-head"><strong>No checks match the current filters.</strong></div><div class="sub">Clear one filter or switch back to All to see the full report.</div></div>';
        return;
      }
      root.innerHTML = filtered.map(item => {
        const status = item.ok ? 'ok' : 'warn';
        const severity = normalizeSeverity(item.severity);
        const category = normalizeCategory(item.category);
        return '<article class="check ' + status + '">' +
          '<div class="check-head">' +
          '<strong>' + htmlEscape(item.title) + '</strong>' +
          '<div class="check-meta">' +
          '<span class="pill ' + status + '">' + htmlEscape(item.status || (item.ok ? 'ok' : 'attention')) + '</span>' +
          '<span class="pill ' + severityClass(severity) + '">' + htmlEscape(severity) + '</span>' +
          '<span class="pill ' + categoryClass(category) + '">' + htmlEscape(category) + '</span>' +
          '</div></div>' +
          '<div class="sub">' + htmlEscape(item.detail) + '</div>' +
          '</article>';
      }).join('');
    }

    function updateFilterSummary(items) {
      const filtered = filterItems(items || []);
      const categoryText = activeFilters.category === 'all' ? 'all categories' : activeFilters.category;
      const severityText = activeFilters.severity === 'all' ? 'all severities' : activeFilters.severity;
      setText('filterSummaryText', 'Showing ' + filtered.length + ' checks for ' + categoryText + ' / ' + severityText + '.');
    }

    function applyFilters(data) {
      const selfCheck = data.selfCheck || {};
      updateFilterSummary(selfCheck.items || []);
      renderChecks(selfCheck.items || []);
      renderIssues(selfCheck.issues || []);
      renderActionItems(selfCheck.issues || []);
    }

    function bindFilters() {
      document.querySelectorAll('button.filter').forEach(btn => {
        btn.onclick = () => {
          const kind = btn.getAttribute('data-filter-kind');
          const value = btn.getAttribute('data-filter-value');
          activeFilters[kind] = value;
          document.querySelectorAll('button.filter[data-filter-kind="' + kind + '"]').forEach(x => x.classList.toggle('active', x === btn));
          applyFilters(bundle);
        };
      });
    }

    function renderBundle(data, source) {
      if (!data) return;
      bundle = data;
      const selfCheck = data.selfCheck || {};
      const hotspot = data.hotspot || {};
      const cert = data.cert || {};
      setText('hostStateText', data.hostState || 'unknown');
      setText('hostIpText', data.hostIp || '(not found)');
      setText('roomText', data.room || '');
      setText('viewersText', data.viewers || 0);
      setText('viewerUrlText', (data.links && data.links.viewerUrl) || '');
      setText('hotspotText', (hotspot.running ? 'running' : 'stopped') + (hotspot.ssid ? ' · ' + hotspot.ssid : ''));
      setText('certText', cert.ready ? 'ready' : (cert.detail || 'not ready'));
      setText('summaryText', (selfCheck.passed || 0) + ' / ' + (selfCheck.total || 0));
      const pill = document.getElementById('summaryPill');
      if (pill) {
        const sev = selfCheck.severityCounts || {};
        const hasWarn = (sev.P0 || 0) + (sev.P1 || 0) + (sev.P2 || 0) > 0;
        pill.className = 'pill ' + (hasWarn ? 'warn' : 'ok');
        pill.textContent = hasWarn ? 'Needs attention' : 'Healthy snapshot';
      }
      updateSummaryPills(selfCheck);
      applyFilters(data);
      updateSyncLabel(source || 'embedded snapshot');
    }

    function loadLiveStatus() {
      const script = document.createElement('script');
      script.src = './share_status.js?ts=' + Date.now();
      script.onload = () => {
        script.remove();
        if (window.__LAN_SHARE_STATUS__) renderBundle(window.__LAN_SHARE_STATUS__, 'share_status.js');
      };
      script.onerror = () => {
        script.remove();
        setText('lastSyncText', 'share_status.js unavailable');
      };
      document.head.appendChild(script);
    }

    window.addEventListener('lan-share-status', (ev) => {
      if (ev && ev.detail) renderBundle(ev.detail, 'share_status.js event');
    });

    document.getElementById('copyViewerBtn').onclick = copyViewerUrl;
    document.getElementById('openShareWizardBtn').onclick = () => { window.location.href = './share_wizard.html'; };
    document.getElementById('openShareCardBtn').onclick = () => { window.location.href = './share_card.html'; };
    bindFilters();
    renderBundle(bundle, 'embedded snapshot');
    window.setInterval(loadLiveStatus, bundle.refreshMs || 2000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) loadLiveStatus(); });
    window.addEventListener('focus', loadLiveStatus);
  </script>
</body>
</html>)HTML";
    return html.str();
}

static std::string BuildDesktopSelfCheckText(std::wstring_view generatedAt,
                                             std::wstring_view hostUrl,
                                             std::wstring_view viewerUrl,
                                             const SelfCheckReport& report) {
    std::ostringstream out;
    out << "LAN Screen Share - Desktop self-check\r\n";
    out << "=====================================\r\n\r\n";
    out << "Generated: " << WideToUtf8(std::wstring(generatedAt)) << "\r\n";
    out << "Host URL: " << WideToUtf8(std::wstring(hostUrl)) << "\r\n";
    out << "Viewer URL: " << WideToUtf8(std::wstring(viewerUrl)) << "\r\n\r\n";
    out << "Summary: " << report.passed << " / " << report.total << " checks passed\r\n";
    out << "Severity: P0=" << report.p0 << ", P1=" << report.p1 << ", P2=" << report.p2 << "\r\n";
    out << "Categories: transport=" << report.certificateCount << ", network=" << report.networkCount << ", sharing=" << report.sharingCount << "\r\n\r\n";
    out << "Checks\r\n";
    out << "------\r\n";
    for (const auto& item : report.items) {
        out << "- [" << item.severity << "][" << item.category << "] " << item.title << ": " << item.status << "\r\n";
        out << "  " << item.detail << "\r\n";
    }
    out << "\r\nOpen issues\r\n";
    out << "-----------\r\n";
    if (report.failures.empty()) {
        out << "- No obvious blocking issue detected.\r\n";
    } else {
        for (const auto& issue : report.failures) {
            out << "- [" << issue.severity << "][" << issue.category << "] " << issue.title << ": " << issue.detail << "\r\n";
        }
    }
    out << "\r\nOperator first actions\r\n";
    out << "----------------------\r\n";
    const auto actions = BuildOperatorFirstActions(report, 3);
    if (actions.empty()) {
        out << "- No urgent first action right now. Share the current bundle or re-run checks after any state change.\r\n";
    } else {
        for (const auto& action : actions) {
            out << "- [" << action.severity << "][" << action.category << "] " << action.title << ": "
                << (action.action.empty() ? action.detail : action.action) << "\r\n";
        }
    }
    out << "\r\nNotes\r\n";
    out << "-----\r\n";
    out << "- This report is generated from the same bundle snapshot used by Share Wizard.\r\n";
    out << "- P0 means blocker, P1 means fix soon, P2 means advisory/fallback.\r\n";
    out << "- Re-run the self-check after host IP, server, hotspot, or sharing state changes.\r\n";
    return out.str();
}

static std::string BuildShareBundleJson(std::wstring_view networkMode,
                                        std::wstring_view hostIp,
                                        int port,
                                        std::wstring_view room,
                                        std::wstring_view token,
                                        std::wstring_view hostState,
                                        std::size_t rooms,
                                        std::size_t viewers,
                                        std::wstring_view hotspotSsid,
                                        std::wstring_view hotspotPassword,
                                        bool hotspotRunning,
                                        bool wifiAdapterPresent,
                                        bool hotspotSupported,
                                        bool wifiDirectApiAvailable,
                                        std::wstring_view wifiDirectAlias,
                                        std::wstring_view hostUrl,
                                        std::wstring_view viewerUrl,
                                        std::wstring_view generatedAt,
                                        bool serverProcessRunning,
                                        bool portReady,
                                        std::wstring_view portDetail,
                                        bool localHealthReady,
                                        std::wstring_view localHealthDetail,
                                        bool hostIpReachable,
                                        std::wstring_view hostIpReachableDetail,
                                        bool lanBindReady,
                                        std::wstring_view lanBindDetail,
                                        int activeIpv4Candidates,
                                        bool selectedIpRecommended,
                                        std::wstring_view adapterHint,
                                        bool embeddedHostReady,
                                        std::wstring_view embeddedHostStatus,
                                        bool firewallReady,
                                        std::wstring_view firewallDetail,
                                        std::wstring_view certDir,
                                        std::wstring_view certFile,
                                        std::wstring_view certKeyFile,
                                        bool certFileExists,
                                        bool certKeyExists,
                                        bool certReady,
                                        std::wstring_view certDetail,
                                        std::wstring_view certExpectedSans) {
    auto q = [](std::wstring_view value) {
        return std::string(1, '"') + JsonEscapeUtf8(WideToUtf8(std::wstring(value))) + std::string(1, '"');
    };

    const bool serverRunning = serverProcessRunning || IsHostStateServerRunning(hostState);
    const bool hostReady = IsHostStateReadyOrLoading(hostState);
    const bool hostSharing = IsHostStateSharing(hostState);
    const bool networkReady = !hostIp.empty() && hostIp != L"(not found)" && hostIp != L"0.0.0.0";
    const bool viewerReady = !viewerUrl.empty();
    const auto report = BuildSelfCheckReport(hostState, hostIp, viewerUrl, viewers, serverProcessRunning,
                                             certReady, certDetail, portReady, portDetail,
                                             localHealthReady, localHealthDetail, hostIpReachable, hostIpReachableDetail,
                                             lanBindReady, lanBindDetail, activeIpv4Candidates, selectedIpRecommended, adapterHint,
                                             embeddedHostReady, embeddedHostStatus, firewallReady, firewallDetail,
                                             wifiAdapterPresent, hotspotSupported, wifiDirectApiAvailable, hotspotRunning, true);

    std::ostringstream json;
    json << "{\n";
    json << R"JSON(  "generatedAt": )JSON" << q(generatedAt) << ",\n";
    json << R"JSON(  "statusVersion": )JSON" << q(generatedAt) << ",\n";
    json << R"JSON(  "refreshMs": 2000,
)JSON";
    json << R"JSON(  "serverRunning": )JSON" << (serverRunning ? "true" : "false") << ",\n";
    json << R"JSON(  "networkMode": )JSON" << q(networkMode) << ",\n";
    json << R"JSON(  "hostIp": )JSON" << q(hostIp) << ",\n";
    json << R"JSON(  "port": )JSON" << port << ",\n";
    json << R"JSON(  "room": )JSON" << q(room) << ",\n";
    json << R"JSON(  "token": )JSON" << q(token) << ",\n";
    json << R"JSON(  "hostState": )JSON" << q(hostState) << ",\n";
    json << R"JSON(  "rooms": )JSON" << rooms << ",\n";
    json << R"JSON(  "viewers": )JSON" << viewers << ",\n";
    json << R"JSON(  "hotspot": {
    "running": )JSON" << (hotspotRunning ? "true" : "false") << ",\n";
    json << R"JSON(    "status": )JSON" << (hotspotRunning ? "\"running\"" : "\"stopped\"") << ",\n";
    json << R"JSON(    "supported": )JSON" << (hotspotSupported ? "true" : "false") << ",\n";
    json << R"JSON(    "ssid": )JSON" << q(hotspotSsid) << ",\n";
    json << R"JSON(    "password": )JSON" << q(hotspotPassword) << "\n";
    json << R"JSON(  },
  "wifi": {
    "adapterPresent": )JSON" << (wifiAdapterPresent ? "true" : "false") << "\n";
    json << R"JSON(  },
  "wifiDirect": {
    "apiAvailable": )JSON" << (wifiDirectApiAvailable ? "true" : "false") << ",\n";
    json << R"JSON(    "sessionAlias": )JSON" << q(wifiDirectAlias) << ",\n";
    json << R"JSON(    "pairingEntry": "ms-settings:connecteddevices",
    "fallbackEntry": "control.exe /name Microsoft.DevicesAndPrinters"
  },
  "cert": {
    "dir": )JSON" << q(certDir) << ",\n";
    json << R"JSON(    "certFile": )JSON" << q(certFile) << ",\n";
    json << R"JSON(    "keyFile": )JSON" << q(certKeyFile) << ",\n";
    json << R"JSON(    "certExists": )JSON" << (certFileExists ? "true" : "false") << ",\n";
    json << R"JSON(    "keyExists": )JSON" << (certKeyExists ? "true" : "false") << ",\n";
    json << R"JSON(    "ready": )JSON" << (certReady ? "true" : "false") << ",\n";
    json << R"JSON(    "detail": )JSON" << q(certDetail) << ",\n";
    json << R"JSON(    "expectedSans": )JSON" << q(certExpectedSans) << ",\n";
    json << R"JSON(    "trustHint": "Plain HTTP mode is active. No browser certificate trust step is required."
  },
  "checks": {
    "serverReady": )JSON" << (serverRunning ? "true" : "false") << ",\n";
    json << R"JSON(    "hostReady": )JSON" << (hostReady ? "true" : "false") << ",\n";
    json << R"JSON(    "hostSharing": )JSON" << (hostSharing ? "true" : "false") << ",\n";
    json << R"JSON(    "portReady": )JSON" << (portReady ? "true" : "false") << ",\n";
    json << R"JSON(    "localHealthReady": )JSON" << (localHealthReady ? "true" : "false") << ",\n";
    json << R"JSON(    "hostIpReachable": )JSON" << (hostIpReachable ? "true" : "false") << ",\n";
    json << R"JSON(    "lanBindReady": )JSON" << (lanBindReady ? "true" : "false") << ",\n";
    json << R"JSON(    "selectedIpRecommended": )JSON" << (selectedIpRecommended ? "true" : "false") << ",\n";
    json << R"JSON(    "embeddedHostReady": )JSON" << (embeddedHostReady ? "true" : "false") << ",\n";
    json << R"JSON(    "networkReady": )JSON" << (networkReady ? "true" : "false") << ",\n";
    json << R"JSON(    "viewerReady": )JSON" << (viewerReady ? "true" : "false") << ",\n";
    json << R"JSON(    "certificateReady": )JSON" << (certReady ? "true" : "false") << ",\n";
    json << R"JSON(    "bundleLiveReady": true
  },
  "runtime": {
    "portDetail": )JSON" << q(portDetail) << ",\n";
    json << R"JSON(    "localHealthDetail": )JSON" << q(localHealthDetail) << ",\n";
    json << R"JSON(    "hostIpReachableDetail": )JSON" << q(hostIpReachableDetail) << ",\n";
    json << R"JSON(    "lanBindDetail": )JSON" << q(lanBindDetail) << ",\n";
    json << R"JSON(    "activeIpv4Candidates": )JSON" << activeIpv4Candidates << ",\n";
    json << R"JSON(    "adapterHint": )JSON" << q(adapterHint) << ",\n";
    json << R"JSON(    "embeddedHostStatus": )JSON" << q(embeddedHostStatus) << ",\n";
    json << R"JSON(    "firewallReady": )JSON" << (firewallReady ? "true" : "false") << ",\n";
    json << R"JSON(    "firewallDetail": )JSON" << q(firewallDetail) << "\n";
    json << R"JSON(  },
  "selfCheck": )JSON" << BuildSelfCheckJson(report) << ",\n";
    json << R"JSON(  "bundle": {
    "statusScript": "./share_status.js",
    "readme": "./share_readme.txt",
    "diagnostics": "./share_diagnostics.txt",
    "desktopSelfCheck": "./desktop_self_check.html",
    "desktopSelfCheckText": "./desktop_self_check.txt"
  },
  "links": {
    "hostUrl": )JSON" << q(hostUrl) << ",\n";
    json << R"JSON(    "viewerUrl": )JSON" << q(viewerUrl) << "\n";
    json << R"JSON(  }
}
)JSON";
    return json.str();
}

static std::string BuildShareWizardHtml(std::string_view bundleJson) {
    std::ostringstream html;
    html << R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <meta name="theme-color" content="#050816"/>
  <title>LAN Screen Share - Share Wizard</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
      background:
        radial-gradient(circle at 12% 10%, rgba(72,214,255,.22), transparent 24%),
        radial-gradient(circle at 84% 8%, rgba(156,183,255,.18), transparent 26%),
        linear-gradient(180deg, #050816 0%, #040507 100%);
      color: #edf2f7;
      position: relative;
      overflow-x: hidden;
    }
    body::before,
    body::after {
      content: "";
      position: fixed;
      border-radius: 999px;
      pointer-events: none;
      filter: blur(12px);
      opacity: .85;
    }
    body::before {
      top: 8vh;
      right: -10vw;
      width: 34vw;
      height: 34vw;
      background: radial-gradient(circle, rgba(72,214,255,.18) 0%, rgba(72,214,255,0) 72%);
    }
    body::after {
      left: -8vw;
      bottom: 10vh;
      width: 28vw;
      height: 28vw;
      background: radial-gradient(circle, rgba(156,183,255,.14) 0%, rgba(156,183,255,0) 72%);
    }
    .page { position: relative; z-index: 1; max-width: 1240px; margin: 0 auto; padding: 28px 24px 88px; }
    .hero { display:grid; grid-template-columns: 1.4fr .9fr; gap:18px; margin-bottom:20px; }
    .card {
      background: rgba(8,12,24,.76);
      border: 1px solid rgba(255,255,255,.1);
      border-radius: 28px;
      padding: 22px;
      box-shadow: 0 24px 68px rgba(0,0,0,.42);
      backdrop-filter: blur(22px);
    }
    h1,h2,h3 { margin:0 0 12px; }
    h1 { font-size: clamp(30px, 5vw, 48px); letter-spacing: -.04em; }
    .sub { color: rgba(201,212,255,.78); line-height:1.6; }
    .pill {
      display:inline-flex;
      align-items:center;
      gap:8px;
      border-radius:999px;
      padding:7px 12px;
      background: rgba(8,12,24,.72);
      border:1px solid rgba(255,255,255,.1);
      color:#dbeafe;
      font-size:12px;
      backdrop-filter: blur(18px);
    }
    .pill.ok { background:rgba(14,40,33,.78); border-color:rgba(143,242,208,.24); color:#d1fae5; }
    .pill.warn { background:rgba(43,29,8,.78); border-color:rgba(255,225,140,.22); color:#fde68a; }
    .pill.p0 { background:rgba(53,18,26,.78); border-color:rgba(255,154,154,.24); color:#fecdd3; }
    .pill.p1 { background:rgba(49,33,19,.78); border-color:rgba(255,225,140,.2); color:#fde68a; }
    .pill.p2 { background:rgba(21,37,60,.78); border-color:rgba(125,211,252,.22); color:#bfdbfe; }
    .pill.certificate { background:rgba(31,24,53,.78); border-color:rgba(167,139,250,.24); color:#ddd6fe; }
    .pill.network { background:rgba(15,37,48,.78); border-color:rgba(34,211,238,.2); color:#bae6fd; }
    .pill.sharing { background:rgba(23,36,54,.78); border-color:rgba(129,140,248,.22); color:#c7d2fe; }
    .grid2 { display:grid; grid-template-columns:1fr 1fr; gap:18px; }
    .grid3 { display:grid; grid-template-columns:repeat(3,1fr); gap:14px; }
    .kv { display:grid; grid-template-columns:140px 1fr; gap:8px 12px; margin-top:12px; }
    .kv div:nth-child(odd) { color: rgba(201,212,255,.62); letter-spacing: .12em; text-transform: uppercase; font-size: 11px; }
    .mono { font-family: ui-monospace, SFMono-Regular, Consolas, monospace; overflow-wrap:anywhere; }
    .box {
      background: rgba(4,9,19,.76);
      border:1px solid rgba(255,255,255,.08);
      border-radius:18px;
      padding:16px;
      box-shadow: inset 0 1px 0 rgba(255,255,255,.03);
    }
    .step { border:1px solid rgba(255,255,255,.08); border-radius:20px; padding:18px; background:rgba(8,12,24,.58); }
    .step.ok { border-color:rgba(143,242,208,.22); background:rgba(12,34,28,.76); }
    .step.warn { border-color:rgba(255,225,140,.22); background:rgba(43,29,8,.76); }
    .title-row { display:flex; gap:10px; align-items:center; justify-content:space-between; }
    .actions { display:flex; flex-wrap:wrap; gap:10px; margin-top:16px; }
    button, a.btn {
      appearance:none;
      text-decoration:none;
      border:1px solid rgba(255,255,255,.12);
      background:rgba(11,20,34,.78);
      color:#fff;
      padding:10px 14px;
      border-radius:999px;
      cursor:pointer;
      display:inline-flex;
      align-items:center;
      gap:8px;
      backdrop-filter: blur(16px);
      transition: background .18s ease, border-color .18s ease, transform .18s ease;
    }
    button:hover, a.btn:hover {
      background: rgba(18,30,52,.92);
      border-color: rgba(125,211,252,.24);
      transform: translateY(-1px);
    }
    button.filter { padding:8px 12px; font-size:12px; }
    button.filter.active { background:#244064; border-color:#7dd3fc; color:#e0f2fe; }
    .small { font-size:13px; color:rgba(201,212,255,.76); line-height:1.6; }
    .qr {
      min-height:300px;
      display:flex;
      align-items:center;
      justify-content:center;
      background:#fff;
      border-radius:24px;
      padding:18px;
      box-shadow: inset 0 0 0 1px rgba(11,20,34,.08);
    }
    .qr svg { width:min(100%, 280px); height:auto; display:block; }
    .livebar { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-bottom:12px; }
    .em { color:#7dd3fc; }
    .diag-grid { display:grid; grid-template-columns:repeat(2, minmax(0,1fr)); gap:12px; margin-top:14px; }
    .diag-item { border:1px solid rgba(255,255,255,.08); border-radius:18px; padding:14px; background:rgba(8,12,24,.58); }
    .diag-item.ok { border-color:rgba(143,242,208,.22); background:rgba(12,34,28,.76); }
    .diag-item.warn { border-color:rgba(255,225,140,.22); background:rgba(43,29,8,.76); }
    .diag-item h3 { font-size:15px; margin-bottom:8px; }
    .checkline { display:flex; align-items:center; justify-content:space-between; gap:12px; }
    .issues, .action-list { margin:0; padding-left:18px; }
    .issues li, .action-list li { margin:10px 0; color:#dbe4f3; }
    .filter-row, .summary-row { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-top:12px; }
    .issues strong, .action-list strong { color:#fff; }
    ol { margin:10px 0 0 18px; padding:0; }
    li { margin:8px 0; }
    .toast {
      position: fixed;
      right: 20px;
      bottom: 20px;
      z-index: 4;
      max-width: min(420px, calc(100vw - 40px));
      padding: 14px 16px;
      border-radius: 18px;
      border: 1px solid rgba(255,255,255,.12);
      background: rgba(6,12,24,.92);
      color: #f4fbff;
      box-shadow: 0 22px 54px rgba(0,0,0,.38);
      backdrop-filter: blur(18px);
      opacity: 0;
      transform: translateY(12px);
      pointer-events: none;
      transition: opacity .18s ease, transform .18s ease;
    }
    .toast[data-visible="true"] { opacity: 1; transform: translateY(0); }
    .toast.ok { border-color: rgba(143,242,208,.24); color: #dffef1; }
    .toast.warn { border-color: rgba(255,225,140,.22); color: #fff0b8; }
    @media (max-width: 980px) {
      .page { padding: 18px 16px 80px; }
      .hero, .grid2, .grid3, .diag-grid { grid-template-columns:1fr; }
      .kv { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <main class="page">
    <section class="hero">
      <div class="card">
        <div class="livebar">
          <div class="pill">Local share wizard + local QR + offline bundle</div>
          <div class="pill">Live auto-refresh: <span id="liveStatusText">enabled</span></div>
          <div class="small">Last sync: <span id="lastSyncText">embedded snapshot</span></div>
        </div>
        <h1>LAN Screen Share Wizard</h1>
        <p class="sub">This page is generated locally by the desktop host. It packages the current room, token, URLs, hotspot details, and Wi-Fi Direct guidance into one offline handoff bundle.</p>
        <p class="small">After handing off the Viewer URL or QR, return to the desktop Dashboard or tray icon to confirm whether the session is now <strong>Ready For Handoff</strong>, <strong>Needs Fix</strong>, or already <strong>Delivered</strong>.</p>
        <div class="kv">
          <div>Mode</div><div id="modeText"></div>
          <div>Host IPv4</div><div id="hostIpText" class="mono"></div>
          <div>Port</div><div id="portText"></div>
          <div>Room</div><div id="roomText" class="mono"></div>
          <div>Host state</div><div id="hostStateText"></div>
          <div>Rooms / Viewers</div><div><span id="roomsText"></span> / <span id="viewersText"></span></div>
          <div>Generated</div><div id="generatedAtText"></div>
        </div>
        <div class="actions">
          <button id="copyViewerBtn" type="button">Copy Viewer URL</button>
          <button id="copyTokenBtn" type="button">Copy Token</button>
          <button id="openShareCardBtn" type="button">Open Share Card</button>
          <a class="btn" href="./desktop_self_check.html" target="_blank" rel="noopener">Open Desktop Self-Check</a>
          <a class="btn" href="./desktop_self_check.txt" target="_blank" rel="noopener">Open Self-Check TXT</a>
          <a class="btn" href="./share_bundle.json" target="_blank" rel="noopener">Open JSON</a>
          <a class="btn" href="./share_diagnostics.txt" target="_blank" rel="noopener">Open Diagnostics</a>
        </div>
      </div>
      <div class="card">
        <h2>Viewer QR</h2>
        <div id="qrMount" class="qr" aria-live="polite">Loading QR...</div>
        <div class="actions">
          <button id="downloadQrBtn" type="button">Download QR SVG</button>
          <button id="copyViewerBtn2" type="button">Copy Viewer URL</button>
        </div>
        <div class="small mono" id="viewerUrlText" style="margin-top:12px;"></div>
      </div>
    </section>

    <section class="grid3" style="margin-bottom:20px;">
      <article class="step" id="stepServer">
        <div class="title-row"><h3>1. Server</h3><span id="serverBadge" class="pill">pending</span></div>
        <div class="small" id="serverNote">The desktop host should keep the local HTTP/WS service running while the viewer joins.</div>
      </article>
      <article class="step" id="stepHost">
        <div class="title-row"><h3>2. Host page</h3><span id="hostBadge" class="pill">pending</span></div>
        <div class="small" id="hostNote">Open the host page in the embedded browser, then start screen sharing from the button inside the host page.</div>
      </article>
      <article class="step" id="stepViewer">
        <div class="title-row"><h3>3. Viewer access</h3><span id="viewerBadge" class="pill">pending</span></div>
        <div class="small" id="viewerNote">The viewer can scan the QR or enter the Viewer URL manually. Plain HTTP mode does not require a browser trust step.</div>
      </article>
    </section>

    <section class="grid2" style="margin-bottom:20px;">
      <article class="card">
        <h2>Diagnostic self-check</h2>
        <div class="small">These cards now consume the exported <span class="mono">selfCheck.items</span> model first, so Share Wizard and desktop self-check stay aligned.</div>
        <div class="small" style="margin-top:10px;">Passed: <span id="checkSummaryText" class="em">0 / 0</span></div>
        <div class="summary-row">
          <span class="pill p0" id="wizP0Pill">P0: 0</span>
          <span class="pill p1" id="wizP1Pill">P1: 0</span>
          <span class="pill p2" id="wizP2Pill">P2: 0</span>
          <span class="pill certificate" id="wizCertPill">Transport: 0</span>
          <span class="pill network" id="wizNetworkPill">Network: 0</span>
          <span class="pill sharing" id="wizSharingPill">Sharing: 0</span>
        </div>
        <div class="filter-row">
          <strong>Category</strong>
          <button class="filter active" type="button" data-filter-kind="category" data-filter-value="all">All</button>
          <button class="filter" type="button" data-filter-kind="category" data-filter-value="certificate">Transport</button>
          <button class="filter" type="button" data-filter-kind="category" data-filter-value="network">Network</button>
          <button class="filter" type="button" data-filter-kind="category" data-filter-value="sharing">Sharing</button>
        </div>
        <div class="filter-row">
          <strong>Severity</strong>
          <button class="filter active" type="button" data-filter-kind="severity" data-filter-value="all">All</button>
          <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P0">P0</button>
          <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P1">P1</button>
          <button class="filter" type="button" data-filter-kind="severity" data-filter-value="P2">P2</button>
        </div>
        <div class="small" id="diagFilterSummaryText" style="margin-top:10px;">Showing all checks.</div>
        <div class="diag-grid" id="diagGrid"></div>
      </article>
      <article class="card">
        <h2>Common failure scenarios</h2>
        <div class="small">The wizard keeps this list focused on the issues that match the current state instead of showing a long static FAQ.</div>
        <ul class="issues" id="failureList"></ul>
        <h3 style="margin-top:18px;">Suggested next actions</h3>
        <div class="small">Use these as the fastest operator workflow before exporting or sharing the bundle again.</div>
        <ul class="action-list" id="actionList"></ul>
      </article>
    </section>

    <section class="grid2">
      <article class="card">
        <h2>Connection paths</h2>
        <div class="box" style="margin-bottom:14px;">
          <h3 style="margin-bottom:8px;">Same LAN / Wi-Fi</h3>
          <ol class="small">
            <li>Join the same router or Wi-Fi as the host.</li>
            <li>Open the Viewer URL or scan the QR.</li>
            <li>No certificate trust step is required in plain HTTP mode.</li>
          </ol>
        </div>
        <div class="box" style="margin-bottom:14px;">
          <h3 style="margin-bottom:8px;">Local Hotspot</h3>
          <div class="kv" style="margin-top:0;">
            <div>SSID</div><div id="ssidText" class="mono"></div>
            <div>Password</div><div id="pwdText" class="mono"></div>
            <div>Status</div><div id="hotspotStatusText"></div>
          </div>
          <ol class="small">
            <li>Start the hotspot from the desktop host or from Windows Mobile Hotspot settings.</li>
            <li>Connect the phone or another PC to that hotspot.</li>
            <li>Open the Viewer URL after the device has an IP on the same local link.</li>
          </ol>
        </div>
        <div class="box">
          <h3 style="margin-bottom:8px;">Wi-Fi Direct</h3>
          <div class="kv" style="margin-top:0;">
            <div>API</div><div id="wifiDirectApiText"></div>
            <div>Alias</div><div id="wifiDirectAliasText" class="mono"></div>
          </div>
          <ol class="small">
            <li>Open the Windows Wi-Fi Direct / device pairing flow from the desktop host.</li>
            <li>Use the alias above to identify the correct session.</li>
            <li>Keep the Viewer URL as the actual media entry even after pairing.</li>
          </ol>
        </div>
      </article>
      <article class="card">
        <h2>Local access & bundle support</h2>
        <div class="box" style="margin-bottom:14px;">
          <div class="kv" style="margin-top:0;">
            <div>Support dir</div><div id="certDirText" class="mono"></div>
            <div>Legacy cert file</div><div id="certFileText" class="mono"></div>
            <div>Legacy key file</div><div id="certKeyText" class="mono"></div>
            <div>Transport</div><div id="certStateText"></div>
          </div>
          <div class="small" id="certHintText" style="margin-top:10px;"></div>
        </div>
        <div class="box">
          <h3 style="margin-bottom:8px;">Bundle files</h3>
          <div class="kv" style="margin-top:0;">
            <div class="mono">share_card.html / share_wizard.html</div>
            <div>Human-friendly handoff pages with live polling from <span class="mono">share_status.js</span>.</div>
)HTML";
    html << R"HTML(
            <div class="mono">share_bundle.json / share_status.js / desktop_self_check.html</div>
            <div>Machine-readable snapshot plus the script reloaded by already-open pages.</div>
            <div class="mono">share_diagnostics.txt / desktop_self_check.txt</div>
            <div>Support-oriented snapshot with self-check summary and targeted troubleshooting tips.</div>
            <div class="mono">viewer_url.txt / hotspot_credentials.txt / share_readme.txt</div>
            <div>Manual fallback material for cases where QR scanning or browser navigation is slow or inconvenient.</div>
          </div>
        </div>
        <div class="actions">
          <button id="copySsidBtn" type="button">Copy SSID</button>
          <button id="copyPwdBtn" type="button">Copy Password</button>
          <a class="btn" href="./share_readme.txt" target="_blank" rel="noopener">Open README</a>
        </div>
      </article>
    </section>
  </main>
  <div id="toast" class="toast" aria-live="polite"></div>

  <script id="bundleJson" type="application/json">)HTML";
    html << bundleJson;
    html << R"HTML(</script>
  <script src="./www/assets/share_card_qr.bundle.js"></script>
  <script>
    let bundle = JSON.parse(document.getElementById('bundleJson').textContent);
    let currentViewerUrl = '';
    const activeFilters = { category: 'all', severity: 'all' };
    let toastTimer = null;

    function setText(id, text) {
      const el = document.getElementById(id);
      if (el) el.textContent = text == null ? '' : String(text);
    }

    function notify(message, tone) {
      const toast = document.getElementById('toast');
      if (!toast) return;
      toast.textContent = String(message || '');
      toast.className = 'toast' + (tone ? ' ' + tone : '');
      toast.setAttribute('data-visible', 'true');
      if (toastTimer) window.clearTimeout(toastTimer);
      toastTimer = window.setTimeout(() => toast.setAttribute('data-visible', 'false'), 2200);
    }

    function htmlEscape(text) {
      return String(text == null ? '' : text)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#39;');
    }

    function normalizeSeverity(v) {
      return v === 'P0' || v === 'P1' ? v : 'P2';
    }

    function normalizeCategory(v) {
      return v === 'certificate' || v === 'network' ? v : 'sharing';
    }

    function pillClassForSeverity(v) {
      return normalizeSeverity(v).toLowerCase();
    }

    function pillClassForCategory(v) {
      return normalizeCategory(v);
    }

    function filterItems(items) {
      return (items || []).filter(item => {
        const categoryOk = activeFilters.category === 'all' || normalizeCategory(item.category) === activeFilters.category;
        const severityOk = activeFilters.severity === 'all' || normalizeSeverity(item.severity) === activeFilters.severity;
        return categoryOk && severityOk;
      });
    }

    function updateWizardSummaryPills(report) {
      const sev = report.severityCounts || {};
      const cat = report.categoryCounts || {};
      setText('wizP0Pill', 'P0: ' + (sev.P0 || 0));
      setText('wizP1Pill', 'P1: ' + (sev.P1 || 0));
      setText('wizP2Pill', 'P2: ' + (sev.P2 || 0));
      setText('wizCertPill', 'Transport: ' + (cat.certificate || 0));
      setText('wizNetworkPill', 'Network: ' + (cat.network || 0));
      setText('wizSharingPill', 'Sharing: ' + (cat.sharing || 0));
    }

    function updateDiagFilterSummary(items) {
      const filtered = filterItems(items || []);
      const categoryText = activeFilters.category === 'all' ? 'all categories' : activeFilters.category;
      const severityText = activeFilters.severity === 'all' ? 'all severities' : activeFilters.severity;
      setText('diagFilterSummaryText', 'Showing ' + filtered.length + ' checks for ' + categoryText + ' / ' + severityText + '.');
    }

    function bindWizardFilters() {
      document.querySelectorAll('button.filter').forEach(btn => {
        btn.onclick = () => {
          const kind = btn.getAttribute('data-filter-kind');
          const value = btn.getAttribute('data-filter-value');
          activeFilters[kind] = value;
          document.querySelectorAll('button.filter[data-filter-kind="' + kind + '"]').forEach(x => x.classList.toggle('active', x === btn));
          renderChecks(bundle);
        };
      });
    }

    async function copyText(text, okLabel) {
      try {
        await navigator.clipboard.writeText(String(text || ''));
        notify(okLabel, 'ok');
      } catch (_) {
        notify('Copy failed. Please copy it manually.', 'warn');
      }
    }

    function updateSyncLabel(source) {
      const now = new Date();
      setText('lastSyncText', source + ' @ ' + now.toLocaleTimeString());
    }

    function downloadQr() {
      try {
        const svg = window.LanShareQr.buildSvgMarkup(currentViewerUrl, { cellSize: 8, margin: 4 });
        const blob = new Blob([svg], { type: 'image/svg+xml;charset=utf-8' });
        const href = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = href;
        a.download = 'viewer_qr.svg';
        document.body.appendChild(a);
        a.click();
        a.remove();
        setTimeout(() => URL.revokeObjectURL(href), 1000);
        notify('QR SVG downloaded', 'ok');
      } catch (err) {
        notify('QR export failed: ' + err, 'warn');
      }
    }

    function renderQr(url) {
      currentViewerUrl = String(url || '');
      const mount = document.getElementById('qrMount');
      try {
        if (!window.LanShareQr) throw new Error('Local QR bundle missing');
        mount.style.background = '#fff';
        mount.style.color = '#111827';
        window.LanShareQr.renderInto(mount, currentViewerUrl, { cellSize: 8, margin: 4 });
      } catch (err) {
        mount.style.background = '#111827';
        mount.style.color = '#edf2f7';
        mount.textContent = 'QR rendering failed. Use the Viewer URL manually.\n' + String(err);
      }
    }

    function setBadge(elOrId, status, label) {
      const el = typeof elOrId === 'string' ? document.getElementById(elOrId) : elOrId;
      if (!el) return;
      el.className = 'pill' + (status === 'ok' ? ' ok' : status === 'warn' ? ' warn' : '');
      el.textContent = label;
    }

    function setCardState(cardId, badgeId, status, note) {
      const card = document.getElementById(cardId);
      if (card) {
        card.classList.remove('ok', 'warn');
        if (status === 'ok' || status === 'warn') card.classList.add(status);
      }
      setBadge(badgeId, status, status === 'ok' ? 'ok' : status === 'warn' ? 'attention' : 'pending');
      const noteMap = { stepServer: 'serverNote', stepHost: 'hostNote', stepViewer: 'viewerNote' };
      const noteEl = document.getElementById(noteMap[cardId] || '');
      if (noteEl && note) noteEl.textContent = note;
    }

    function renderFailures(items) {
      const root = document.getElementById('failureList');
      if (!root) return;
      const filtered = filterItems((items || []).map(item => ({
        title: item.title,
        detail: item.detail,
        severity: normalizeSeverity(item.severity),
        category: normalizeCategory(item.category)
      })));
      if (!filtered.length) {
        root.innerHTML = '<li><strong>No matching issue in this filter view.</strong> Switch back to All to review the full troubleshooting list.</li>';
        return;
      }
      root.innerHTML = filtered.map(item => '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(item.detail) +
        ' <span class="pill ' + pillClassForSeverity(item.severity) + '">' + htmlEscape(item.severity) + '</span>' +
        ' <span class="pill ' + pillClassForCategory(item.category) + '">' + htmlEscape(item.category) + '</span></li>').join('');
    }

    function sortIssuesByPriority(items) {
      return (items || []).slice().sort((a, b) => {
        const sevOrder = { P0: 0, P1: 1, P2: 2 };
        const ak = sevOrder[normalizeSeverity(a.severity)] ?? 9;
        const bk = sevOrder[normalizeSeverity(b.severity)] ?? 9;
        if (ak !== bk) return ak - bk;
        const ac = normalizeCategory(a.category);
        const bc = normalizeCategory(b.category);
        if (ac !== bc) return ac.localeCompare(bc);
        return String(a.title || '').localeCompare(String(b.title || ''));
      });
    }

    function renderActionList(actions) {
      const root = document.getElementById('actionList');
      if (!root) return;
      if (!actions.length) {
        root.innerHTML = '<li><strong>No urgent action needed.</strong> You can share the bundle as-is, or use Re-run Checks from the desktop host after any state change.</li>';
        return;
      }
      root.innerHTML = actions.map(item => {
        const detail = item.action || item.detail || '';
        const secondary = item.action && item.detail && item.action !== item.detail
          ? '<div class="small" style="margin-top:6px;">Why: ' + htmlEscape(item.detail) + '</div>'
          : '';
        return '<li><strong>' + htmlEscape(item.title) + ':</strong> ' + htmlEscape(detail) +
          secondary +
          ' <span class="pill ' + pillClassForSeverity(item.severity) + '">' + htmlEscape(normalizeSeverity(item.severity)) + '</span>' +
          ' <span class="pill ' + pillClassForCategory(item.category) + '">' + htmlEscape(normalizeCategory(item.category)) + '</span></li>';
      }).join('');
    }
)HTML";
    html << R"HTML(

    function indexSelfCheckItems(report) {
      const map = Object.create(null);
      ((report && report.items) || []).forEach(item => { if (item && item.id) map[item.id] = item; });
      return map;
    }

    function getSelfCheckItem(map, id, title, ok, okDetail, warnDetail, severity, category) {
      if (map[id]) return map[id];
      return { id, title, ok: !!ok, status: ok ? 'ok' : 'attention', detail: ok ? okDetail : warnDetail, severity: normalizeSeverity(severity), category: normalizeCategory(category) };
    }

    function renderDiagGrid(items) {
      const root = document.getElementById('diagGrid');
      if (!root) return;
      const filtered = filterItems(items || []);
      if (!filtered.length) {
        root.innerHTML = '<div class="diag-item warn"><h3>No checks match the current filters</h3><div class="small">Clear one filter or switch back to All to review the full report.</div></div>';
        return;
      }
      root.innerHTML = filtered.map(item => {
        const cls = item.ok ? 'ok' : 'warn';
        const badge = htmlEscape(item.status || (item.ok ? 'ok' : 'attention'));
        const severity = normalizeSeverity(item.severity);
        const category = normalizeCategory(item.category);
        return '<article class="diag-item ' + cls + '">' +
          '<div class="checkline"><h3>' + htmlEscape(item.title) + '</h3><div style="display:flex; gap:8px; flex-wrap:wrap; justify-content:flex-end;">' +
          '<span class="pill ' + cls + '">' + badge + '</span>' +
          '<span class="pill ' + pillClassForSeverity(severity) + '">' + htmlEscape(severity) + '</span>' +
          '<span class="pill ' + pillClassForCategory(category) + '">' + htmlEscape(category) + '</span>' +
          '</div></div>' +
          '<div class="small">' + htmlEscape(item.detail) + '</div>' +
          '</article>';
      }).join('');
    }

    function buildSuggestedActions(data, report) {
      const issues = sortIssuesByPriority(Array.isArray(report.issues) ? report.issues.slice() : []);
      const actions = issues
        .filter(item => !!(item && (item.action || item.detail)))
        .map(item => ({
          title: item.title,
          detail: item.detail,
          action: item.action || item.detail,
          severity: normalizeSeverity(item.severity),
          category: normalizeCategory(item.category)
        }));
      if (actions.length) return actions.slice(0, 5);
      if ((data.viewers || 0) > 0) {
        return [{
          title: 'Viewer already connected',
          detail: 'A viewer is already attached. You can keep sharing, or still export a refreshed bundle for other devices.',
          action: 'Keep sharing, or export a refreshed bundle for any new device that still needs the latest Viewer URL and QR.',
          severity: 'P2',
          category: 'sharing'
        }];
      }
      return [{
        title: 'Healthy bundle',
        detail: 'The current exported bundle looks consistent. Share Wizard, Share Card, and Desktop Self-Check are aligned to the same self-check snapshot.',
        action: 'Share the current bundle as-is, or use Re-run Checks after any change to server, IP, hotspot, or sharing state.',
        severity: 'P2',
        category: 'sharing'
      }];
    }
)HTML";
    html << R"HTML(

    function renderChecks(data) {
      const cert = data.cert || {};
      const report = data.selfCheck || {};
      const map = indexSelfCheckItems(report);
      const serverItem = getSelfCheckItem(map, 'server-reachability', 'Server reachability', !!(data.checks && data.checks.serverReady), 'The desktop host reports the HTTP/WS service as running.', 'The desktop host is not reporting a running service. Press Start and wait for the host page to load.', 'P0', 'sharing');
      const hostItem = getSelfCheckItem(map, 'host-sharing-state', 'Host sharing state', String(data.hostState || '').toLowerCase() === 'sharing', 'Screen sharing is active in the embedded host page.', 'The host page is not sharing yet.', 'P1', 'sharing');
      const viewerItem = getSelfCheckItem(map, 'viewer-entry-url', 'Viewer entry URL', !!(data.links && data.links.viewerUrl), 'Viewer URL and QR target are present in the bundle.', 'Viewer URL is empty. Re-export after host IP and port are valid.', 'P1', 'sharing');
      const networkItem = getSelfCheckItem(map, 'host-network', 'Host network', !!data.hostIp && data.hostIp !== '(not found)' && data.hostIp !== '0.0.0.0', 'Host IPv4 looks usable.', 'Host IPv4 is missing. Refresh the IP, or start hotspot / join a LAN before exporting again.', 'P0', 'network');

      setCardState('stepServer', 'serverBadge', serverItem.ok ? 'ok' : 'warn', serverItem.detail);
      setCardState('stepHost', 'hostBadge', hostItem.ok ? 'ok' : 'warn', hostItem.detail);
      if ((data.viewers || 0) > 0) {
        setCardState('stepViewer', 'viewerBadge', 'ok', 'A viewer is already connected. Current viewers: ' + data.viewers + '.');
      } else if (!networkItem.ok) {
        setCardState('stepViewer', 'viewerBadge', 'warn', networkItem.detail);
      } else {
        setCardState('stepViewer', 'viewerBadge', viewerItem.ok ? 'warn' : 'warn', viewerItem.ok ? 'Viewer URL is ready. Scan the QR or open the URL manually on the other device.' : viewerItem.detail);
      }

      const preferredOrder = ['server-reachability', 'listen-port', 'server-health-endpoint', 'lan-bind-address', 'lan-entry-endpoint', 'adapter-selection', 'embedded-host-runtime', 'host-sharing-state', 'host-network', 'viewer-entry-url', 'transport-mode', 'wifi-adapter', 'hotspot-control', 'live-bundle-refresh'];
      const ordered = [];
      preferredOrder.forEach(id => { if (map[id]) ordered.push(map[id]); });
      ((report.items) || []).forEach(item => { if (item && !ordered.some(x => x.id === item.id)) ordered.push(item); });
      const summaryTotal = report.total || ordered.length || 0;
      const summaryPassed = report.passed || ordered.filter(item => item && item.ok).length;
      setText('checkSummaryText', summaryPassed + ' / ' + summaryTotal);
      updateWizardSummaryPills(report);
      updateDiagFilterSummary(ordered);
      renderDiagGrid(ordered);

      let failures = Array.isArray(report.issues) ? report.issues.slice() : [];
      if (!navigator.onLine) failures.push({ title: 'Browser reports offline mode', severity: 'P1', category: 'network', detail: 'The pages are local, but the viewer still needs local network reachability to the host IP. Disable airplane mode or reconnect to LAN / hotspot.' });
      renderFailures(failures);
      renderActionList(buildSuggestedActions(data, report));
    }
)HTML";
    html << R"HTML(

    function renderBundle(data, source) {
      if (!data || !data.links) return;
      bundle = data;
      const hotspot = data.hotspot || {};
      const wifiDirect = data.wifiDirect || {};
      const cert = data.cert || {};
      setText('modeText', data.networkMode || 'unknown');
      setText('hostIpText', data.hostIp || '(not found)');
      setText('portText', data.port || '');
      setText('roomText', data.room || '');
      setText('hostStateText', data.hostState || 'unknown');
      setText('roomsText', data.rooms || 0);
      setText('viewersText', data.viewers || 0);
      setText('generatedAtText', data.generatedAt || '');
      setText('viewerUrlText', (data.links && data.links.viewerUrl) || '');
      setText('ssidText', hotspot.ssid || '(not configured)');
      setText('pwdText', hotspot.password || '(not configured)');
      setText('hotspotStatusText', hotspot.running ? 'running' : 'stopped');
      setText('wifiDirectApiText', wifiDirect.apiAvailable ? 'available' : 'not detected');
      setText('wifiDirectAliasText', wifiDirect.sessionAlias || '(not set)');
      setText('certDirText', cert.dir || '(cert dir unknown)');
      setText('certFileText', cert.certFile || '(missing)');
      setText('certKeyText', cert.keyFile || '(missing)');
      setText('certStateText', cert.ready ? 'plain HTTP mode active' : 'mode detail unavailable');
      setText('certHintText', cert.trustHint || 'Plain HTTP mode is active. No browser trust step is required.');
      setText('liveStatusText', (data.bundle && data.bundle.statusScript) ? 'enabled' : 'disabled');
      updateSyncLabel(source || 'embedded snapshot');
      renderQr((data.links && data.links.viewerUrl) || '');
      renderChecks(data);
    }

    function loadLiveStatus() {
      const script = document.createElement('script');
      script.src = './share_status.js?ts=' + Date.now();
      script.async = true;
      script.onload = () => {
        if (window.__LAN_SHARE_STATUS__) renderBundle(window.__LAN_SHARE_STATUS__, 'share_status.js');
        script.remove();
      };
      script.onerror = () => {
        setText('lastSyncText', 'share_status.js unavailable');
        script.remove();
      };
      document.head.appendChild(script);
    }

    window.addEventListener('lan-share-status', (ev) => {
      if (ev && ev.detail) renderBundle(ev.detail, 'share_status.js event');
    });

    document.getElementById('copyViewerBtn').onclick = () => copyText((bundle.links && bundle.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('copyViewerBtn2').onclick = () => copyText((bundle.links && bundle.links.viewerUrl) || '', 'Viewer URL copied');
    document.getElementById('copyTokenBtn').onclick = () => copyText(bundle.token || '', 'Token copied');
    document.getElementById('openShareCardBtn').onclick = () => { window.location.href = './share_card.html'; };
    document.getElementById('copySsidBtn').onclick = () => copyText((bundle.hotspot && bundle.hotspot.ssid) || '', 'SSID copied');
    document.getElementById('copyPwdBtn').onclick = () => copyText((bundle.hotspot && bundle.hotspot.password) || '', 'Password copied');
    document.getElementById('downloadQrBtn').onclick = downloadQr;

    bindWizardFilters();
    renderBundle(bundle, 'embedded snapshot');
    window.setInterval(loadLiveStatus, bundle.refreshMs || 2000);
    document.addEventListener('visibilitychange', () => { if (!document.hidden) loadLiveStatus(); });
    window.addEventListener('focus', loadLiveStatus);
  </script>
</body>
</html>)HTML";
    return html.str();
}

static std::string BuildShareStatusJs(std::string_view bundleJson) {
    std::string js;
    js.reserve(bundleJson.size() + 256);
    js += "window.__LAN_SHARE_STATUS__ = ";
    js.append(bundleJson.begin(), bundleJson.end());
    js += ";\n";
    js += "window.__LAN_SHARE_STATUS_LOADED_AT__ = new Date().toISOString();\n";
    js += "try { window.dispatchEvent(new CustomEvent('lan-share-status', { detail: window.__LAN_SHARE_STATUS__ })); } catch (_) {}\n";
    return js;
}

static std::string BuildShareDiagnosticsText(std::wstring_view generatedAt,
                                             std::wstring_view mode,
                                             std::wstring_view hostIp,
                                             int port,
                                             std::wstring_view room,
                                             std::wstring_view hostState,
                                             std::size_t rooms,
                                             std::size_t viewers,
                                             std::wstring_view hotspotStatus,
                                             std::wstring_view hotspotSsid,
                                             bool wifiAdapterPresent,
                                             bool hotspotSupported,
                                             bool wifiDirectApiAvailable,
                                             std::wstring_view hostUrl,
                                             std::wstring_view viewerUrl,
                                             bool serverProcessRunning,
                                             bool portReady,
                                             std::wstring_view portDetail,
                                             bool localHealthReady,
                                             std::wstring_view localHealthDetail,
                                             bool hostIpReachable,
                                             std::wstring_view hostIpReachableDetail,
                                             bool lanBindReady,
                                             std::wstring_view lanBindDetail,
                                             int activeIpv4Candidates,
                                             bool selectedIpRecommended,
                                             std::wstring_view adapterHint,
                                             bool embeddedHostReady,
                                             std::wstring_view embeddedHostStatus,
                                             bool firewallReady,
                                             std::wstring_view firewallDetail,
                                             std::wstring_view certDir,
                                             std::wstring_view certFile,
                                             std::wstring_view certKeyFile,
                                             bool certFileExists,
                                             bool certKeyExists,
                                             bool certReady,
                                             std::wstring_view certDetail,
                                             std::wstring_view certExpectedSans) {
    const auto report = BuildSelfCheckReport(hostState, hostIp, viewerUrl, viewers, serverProcessRunning,
                                             certReady, certDetail, portReady, portDetail,
                                             localHealthReady, localHealthDetail, hostIpReachable, hostIpReachableDetail,
                                             lanBindReady, lanBindDetail, activeIpv4Candidates, selectedIpRecommended, adapterHint,
                                             embeddedHostReady, embeddedHostStatus, firewallReady, firewallDetail,
                                             wifiAdapterPresent, hotspotSupported, wifiDirectApiAvailable,
                                             hotspotStatus == L"running", true);

    std::ostringstream out;
    out << "LAN Screen Share diagnostics\r\n";
    out << "============================\r\n\r\n";
    out << "Generated: " << WideToUtf8(std::wstring(generatedAt)) << "\r\n";
    out << "Mode: " << WideToUtf8(std::wstring(mode)) << "\r\n";
    out << "Host IP: " << WideToUtf8(std::wstring(hostIp)) << "\r\n";
    out << "Port: " << port << "\r\n";
    out << "Room: " << WideToUtf8(std::wstring(room)) << "\r\n";
    out << "Host State: " << WideToUtf8(std::wstring(hostState)) << "\r\n";
    out << "Rooms / Viewers: " << rooms << " / " << viewers << "\r\n\r\n";
    out << "Hotspot: " << WideToUtf8(std::wstring(hotspotStatus)) << "\r\n";
    out << "Hotspot SSID: " << WideToUtf8(std::wstring(hotspotSsid)) << "\r\n";
    out << "Wi-Fi adapter present: " << (wifiAdapterPresent ? "yes" : "no") << "\r\n";
    out << "Hotspot supported: " << (hotspotSupported ? "yes" : "no") << "\r\n";
    out << "Wi-Fi Direct API: " << (wifiDirectApiAvailable ? "available" : "not detected") << "\r\n\r\n";
    out << "Certificate files\r\n";
    out << "-----------------\r\n";
    out << "Cert dir: " << WideToUtf8(std::wstring(certDir)) << "\r\n";
    out << "Cert file: " << WideToUtf8(std::wstring(certFile)) << " (" << (certFileExists ? "present" : "missing") << ")\r\n";
    out << "Key file: " << WideToUtf8(std::wstring(certKeyFile)) << " (" << (certKeyExists ? "present" : "missing") << ")\r\n";
    out << "Certificate ready: " << (certReady ? "yes" : "no") << "\r\n";
    out << "Certificate detail: " << WideToUtf8(std::wstring(certDetail)) << "\r\n";
    out << "Expected SANs: " << WideToUtf8(std::wstring(certExpectedSans)) << "\r\n";
    out << "Transport note: Plain HTTP mode is active. No browser certificate trust step is required.\r\n\r\n";
    out << "Host URL: " << WideToUtf8(std::wstring(hostUrl)) << "\r\n";
    out << "Viewer URL: " << WideToUtf8(std::wstring(viewerUrl)) << "\r\n\r\n";
    out << "Runtime checks\r\n";
    out << "--------------\r\n";
    out << "Server process running: " << (serverProcessRunning ? "yes" : "no") << "\r\n";
    out << "Listen port ready: " << (portReady ? "yes" : "no") << "\r\n";
    out << "Port detail: " << WideToUtf8(std::wstring(portDetail)) << "\r\n";
    out << "Local /health ready: " << (localHealthReady ? "yes" : "no") << "\r\n";
    out << "Health detail: " << WideToUtf8(std::wstring(localHealthDetail)) << "\r\n";
    out << "LAN bind ready: " << (lanBindReady ? "yes" : "no") << "\r\n";
    out << "Bind detail: " << WideToUtf8(std::wstring(lanBindDetail)) << "\r\n";
    out << "Selected host IP reachable: " << (hostIpReachable ? "yes" : "no") << "\r\n";
    out << "Host IP reachability detail: " << WideToUtf8(std::wstring(hostIpReachableDetail)) << "\r\n";
    out << "Active IPv4 candidates: " << activeIpv4Candidates << "\r\n";
    out << "Selected IP recommended: " << (selectedIpRecommended ? "yes" : "no") << "\r\n";
    out << "Adapter hint: " << WideToUtf8(std::wstring(adapterHint)) << "\r\n";
    out << "Embedded host ready: " << (embeddedHostReady ? "yes" : "no") << "\r\n";
    out << "Embedded host status: " << WideToUtf8(std::wstring(embeddedHostStatus)) << "\r\n";
    out << "Firewall ready: " << (firewallReady ? "yes" : "no") << "\r\n";
    out << "Firewall detail: " << WideToUtf8(std::wstring(firewallDetail)) << "\r\n\r\n";
    out << "Self-check summary\r\n";
    out << "-------------------\r\n";
    out << "Passed: " << report.passed << " / " << report.total << "\r\n";
    out << "Severity: P0=" << report.p0 << ", P1=" << report.p1 << ", P2=" << report.p2 << "\r\n";
    out << "Categories: transport=" << report.certificateCount << ", network=" << report.networkCount << ", sharing=" << report.sharingCount << "\r\n";
    for (const auto& item : report.items) {
        out << "- [" << item.severity << "][" << item.category << "] " << item.title << ": " << item.status << "\r\n";
        out << "  " << item.detail << "\r\n";
    }
    out << "\r\nCommon failure scenarios\r\n";
    out << "------------------------\r\n";
    if (report.failures.empty()) {
        out << "- No obvious blocking issue detected.\r\n";
    } else {
        for (const auto& issue : report.failures) {
            out << "- [" << issue.severity << "][" << issue.category << "] " << issue.title << ": " << issue.detail << "\r\n";
        }
    }
    out << "\r\nOperator first actions\r\n";
    out << "----------------------\r\n";
    const auto actions = BuildOperatorFirstActions(report, 3);
    if (actions.empty()) {
        out << "- No urgent first action right now. Share the current bundle or re-run checks after any state change.\r\n";
    } else {
        for (const auto& action : actions) {
            out << "- [" << action.severity << "][" << action.category << "] " << action.title << ": "
                << (action.action.empty() ? action.detail : action.action) << "\r\n";
        }
    }
    out << "\r\nLive bundle notes\r\n";
    out << "-----------------\r\n";
    out << "- share_card.html and share_wizard.html poll share_status.js every ~2 seconds.\r\n";
    out << "- desktop_self_check.html uses the same exported state snapshot and also auto-refreshes from share_status.js.\r\n";
    out << "- share_bundle.json is the machine-readable snapshot for this export.\r\n";
    out << "- If an already-open page looks stale, refocus the page or reopen it from the bundle folder.\r\n";
    return out.str();
}

static std::string BuildShareReadmeText(std::wstring_view hostUrl,
                                        std::wstring_view viewerUrl,
                                        std::wstring_view hotspotSsid,
                                        std::wstring_view hotspotPassword,
                                        bool hotspotRunning,
                                        std::wstring_view wifiDirectAlias) {
    std::ostringstream out;
    out << "LAN Screen Share bundle\r\n";
    out << "========================\r\n\r\n";
    out << "Viewer URL:\r\n" << WideToUtf8(std::wstring(viewerUrl)) << "\r\n\r\n";
    out << "Host URL:\r\n" << WideToUtf8(std::wstring(hostUrl)) << "\r\n\r\n";
    out << "Hotspot SSID: " << WideToUtf8(std::wstring(hotspotSsid.empty() ? L"(not configured)" : hotspotSsid)) << "\r\n";
    out << "Hotspot Password: " << WideToUtf8(std::wstring(hotspotPassword.empty() ? L"(not configured)" : hotspotPassword)) << "\r\n";
    out << "Hotspot Status: " << (hotspotRunning ? "running" : "stopped") << "\r\n";
    out << "Wi-Fi Direct Alias: " << WideToUtf8(std::wstring(wifiDirectAlias)) << "\r\n\r\n";
    out << "Recommended flows:\r\n";
    out << "1. Same LAN/Wi-Fi: connect both devices to the same local network, then open the Viewer URL.\r\n";
    out << "2. Hotspot: start the local hotspot first, join the hotspot from the other device, then open the Viewer URL.\r\n";
    out << "3. Wi-Fi Direct: complete Windows pairing first, use the alias above to identify the target session, then keep the Viewer URL as the media entry.\r\n\r\n";
    out << "Transport note:\r\n";
    out << "- Plain HTTP mode is active. No browser certificate trust step is required.\r\n\r\n";
    out << "Live bundle behavior:\r\n";
    out << "- share_card.html, share_wizard.html, and desktop_self_check.html auto-refresh from share_status.js while they stay open.\r\n";
    out << "- share_diagnostics.txt records the latest exported state snapshot for support.\r\n";
    out << "- desktop_self_check.txt is the plain-text version of the desktop operator report.\r\n\r\n";
    return out.str();
}


bool ExportShareArtifacts(const ShareArtifactWriteRequest& request,
                          ShareArtifactWriteResult* result) {
  std::error_code ec;
  const fs::path assetDir = request.outputDir / L"www" / L"assets";
  fs::create_directories(assetDir, ec);
  if (ec) {
    if (result) result->errorMessage = L"Create share bundle dir failed: " + Utf8ToWide(ec.message());
    return false;
  }

  if (!request.qrAssetSource.empty() && fs::exists(request.qrAssetSource)) {
    fs::copy_file(request.qrAssetSource,
                  assetDir / L"share_card_qr.bundle.js",
                  fs::copy_options::overwrite_existing,
                  ec);
    ec.clear();
  }

  const auto hostUrl = BuildHostUrl(request.session);
  const auto viewerUrl = BuildViewerUrl(request.session);
  const auto report = BuildSelfCheckReport(request.session.hostPageState,
                                           request.session.hostIp,
                                           viewerUrl,
                                           request.session.lastViewers,
                                           request.health.serverProcessRunning,
                                           request.cert.ready,
                                           request.cert.detail,
                                           request.health.portReady,
                                           request.health.portDetail,
                                           request.health.localHealthReady,
                                           request.health.localHealthDetail,
                                           request.health.hostIpReachable,
                                           request.health.hostIpReachableDetail,
                                           request.health.lanBindReady,
                                           request.health.lanBindDetail,
                                           static_cast<int>(request.health.activeIpv4Candidates),
                                           request.health.selectedIpRecommended,
                                           request.health.adapterHint,
                                           request.health.embeddedHostReady,
                                           request.health.embeddedHostStatus,
                                           request.health.firewallReady,
                                           request.health.firewallDetail,
                                           request.session.wifiAdapterPresent,
                                           request.session.hotspotSupported,
                                           request.session.wifiDirectApiAvailable,
                                           request.session.hotspotRunning,
                                           request.liveReady);

  const std::string bundleJson = BuildShareBundleJson(
      request.session.networkMode.empty() ? L"unknown" : request.session.networkMode,
      request.session.hostIp,
      request.session.port,
      request.session.room,
      request.session.token,
      request.session.hostPageState,
      request.session.lastRooms,
      request.session.lastViewers,
      request.session.hotspotSsid,
      request.session.hotspotPassword,
      request.session.hotspotRunning,
      request.session.wifiAdapterPresent,
      request.session.hotspotSupported,
      request.session.wifiDirectApiAvailable,
      request.session.wifiDirectAlias,
      hostUrl,
      viewerUrl,
      request.generatedAt,
      request.health.serverProcessRunning,
      request.health.portReady,
      request.health.portDetail,
      request.health.localHealthReady,
      request.health.localHealthDetail,
      request.health.hostIpReachable,
      request.health.hostIpReachableDetail,
      request.health.lanBindReady,
      request.health.lanBindDetail,
      static_cast<int>(request.health.activeIpv4Candidates),
      request.health.selectedIpRecommended,
      request.health.adapterHint,
      request.health.embeddedHostReady,
      request.health.embeddedHostStatus,
      request.health.firewallReady,
      request.health.firewallDetail,
      request.cert.certDir.wstring(),
      request.cert.certFile.wstring(),
      request.cert.keyFile.wstring(),
      request.cert.certExists,
      request.cert.keyExists,
      request.cert.ready,
      request.cert.detail,
      request.cert.expectedSans);

  const fs::path shareCard = request.outputDir / L"share_card.html";
  const fs::path shareWizard = request.outputDir / L"share_wizard.html";
  const fs::path bundleJsonFile = request.outputDir / L"share_bundle.json";
  const fs::path statusJsFile = request.outputDir / L"share_status.js";
  const fs::path diagnosticsFile = request.outputDir / L"share_diagnostics.txt";
  const fs::path desktopSelfCheckHtmlFile = request.outputDir / L"desktop_self_check.html";
  const fs::path desktopSelfCheckTxtFile = request.outputDir / L"desktop_self_check.txt";
  const fs::path viewerUrlFile = request.outputDir / L"viewer_url.txt";
  const fs::path hotspotCredFile = request.outputDir / L"hotspot_credentials.txt";
  const fs::path readmeFile = request.outputDir / L"share_readme.txt";

  if (!WriteUtf8File(shareCard, BuildShareCardHtml(request.session.networkMode,
                                                   request.session.hostIp,
                                                   request.session.port,
                                                   request.session.room,
                                                   request.session.token,
                                                   request.session.hostPageState,
                                                   request.session.lastRooms,
                                                   request.session.lastViewers,
                                                   request.session.hotspotSsid,
                                                   request.session.hotspotPassword,
                                                   request.session.hotspotRunning,
                                                   request.session.wifiDirectApiAvailable,
                                                   hostUrl,
                                                   viewerUrl,
                                                   bundleJson))) {
    if (result) result->errorMessage = L"Write share_card.html failed";
    return false;
  }
  if (!WriteUtf8File(shareWizard, BuildShareWizardHtml(bundleJson))) {
    if (result) result->errorMessage = L"Write share_wizard.html failed";
    return false;
  }
  if (!WriteUtf8File(bundleJsonFile, bundleJson)) {
    if (result) result->errorMessage = L"Write share_bundle.json failed";
    return false;
  }

  WriteUtf8File(statusJsFile, BuildShareStatusJs(bundleJson));
  WriteUtf8File(diagnosticsFile, BuildShareDiagnosticsText(request.generatedAt,
                                                           request.session.networkMode,
                                                           request.session.hostIp,
                                                           request.session.port,
                                                           request.session.room,
                                                           request.session.hostPageState,
                                                           request.session.lastRooms,
                                                           request.session.lastViewers,
                                                           request.session.hotspotStatus,
                                                           request.session.hotspotSsid,
                                                           request.session.wifiAdapterPresent,
                                                           request.session.hotspotSupported,
                                                           request.session.wifiDirectApiAvailable,
                                                           hostUrl,
                                                           viewerUrl,
                                                           request.health.serverProcessRunning,
                                                           request.health.portReady,
                                                           request.health.portDetail,
                                                           request.health.localHealthReady,
                                                           request.health.localHealthDetail,
                                                           request.health.hostIpReachable,
                                                           request.health.hostIpReachableDetail,
                                                           request.health.lanBindReady,
                                                           request.health.lanBindDetail,
                                                           static_cast<int>(request.health.activeIpv4Candidates),
                                                           request.health.selectedIpRecommended,
                                                           request.health.adapterHint,
                                                           request.health.embeddedHostReady,
                                                           request.health.embeddedHostStatus,
                                                           request.health.firewallReady,
                                                           request.health.firewallDetail,
                                                           request.cert.certDir.wstring(),
                                                           request.cert.certFile.wstring(),
                                                           request.cert.keyFile.wstring(),
                                                           request.cert.certExists,
                                                           request.cert.keyExists,
                                                           request.cert.ready,
                                                           request.cert.detail,
                                                           request.cert.expectedSans));
  WriteUtf8File(desktopSelfCheckHtmlFile, BuildDesktopSelfCheckHtml(bundleJson));
  WriteUtf8File(desktopSelfCheckTxtFile, BuildDesktopSelfCheckText(request.generatedAt, hostUrl, viewerUrl, report));
  WriteUtf8File(viewerUrlFile, WideToUtf8(viewerUrl) + "\r\n");
  WriteUtf8File(hotspotCredFile,
                std::string("SSID: ") + WideToUtf8(request.session.hotspotSsid) + "\r\nPassword: " +
                WideToUtf8(request.session.hotspotPassword) + "\r\nStatus: " +
                WideToUtf8(request.session.hotspotStatus) + "\r\n");
  WriteUtf8File(readmeFile, BuildShareReadmeText(hostUrl,
                                                 viewerUrl,
                                                 request.session.hotspotSsid,
                                                 request.session.hotspotPassword,
                                                 request.session.hotspotRunning,
                                                 request.session.wifiDirectAlias));

  if (result) {
    result->shareCardPath = shareCard;
    result->shareWizardPath = shareWizard;
    result->bundleJsonPath = bundleJsonFile;
    result->desktopSelfCheckPath = desktopSelfCheckHtmlFile;
    result->errorMessage.clear();
  }
  return true;
}

} // namespace lan::runtime
