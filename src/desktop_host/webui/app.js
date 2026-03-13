(function () {
  const state = {};
  let activeTab = "dashboard";
  let sessionDirty = false;
  let hotspotDirty = false;
  let previewUrl = "";
  let previewLoaded = false;

  function $(id) {
    return document.getElementById(id);
  }

  function send(message) {
    if (!window.chrome || !window.chrome.webview || typeof window.chrome.webview.postMessage !== "function") {
      $("bridgeStatus").textContent = "Bridge unavailable";
      return;
    }
    window.chrome.webview.postMessage(JSON.stringify({ source: "admin-shell", ...message }));
  }

  function setPairs(id, entries) {
    const root = $(id);
    root.innerHTML = entries.map(([label, value]) => {
      const safeValue = value === undefined || value === null || value === "" ? "-" : String(value);
      return "<div class=\"row\"><dt>" + label + "</dt><dd>" + safeValue + "</dd></div>";
    }).join("");
  }

  function normalizeTab(tab) {
    const value = String(tab || "").toLowerCase();
    return ["dashboard", "session", "network", "sharing", "monitor", "diagnostics", "settings"].includes(value)
      ? value
      : "dashboard";
  }

  function setPreviewBadge(label, tone) {
    const badge = $("hostPreviewBadge");
    badge.textContent = label;
    badge.className = "status status-inline " + tone;
  }

  function updateHostPreview(payload) {
    const frame = $("hostPreviewFrame");
    const empty = $("hostPreviewEmpty");
    const title = $("hostPreviewEmptyTitle");
    const body = $("hostPreviewEmptyBody");
    const note = $("hostPreviewNote");
    const hostState = String(payload.hostState || "").toLowerCase();
    const hostUrl = payload.serverRunning ? String(payload.hostUrl || "") : "";

    if (!hostUrl) {
      if (previewUrl) {
        frame.src = "about:blank";
      }
      previewUrl = "";
      previewLoaded = false;
      empty.hidden = false;
      title.textContent = "Host Preview Unavailable";
      body.textContent = payload.serverRunning
        ? "The service is up, but the Host URL is still empty. Apply session values and refresh the snapshot."
        : "Start the local service first, then the embedded Host page can load inside this panel.";
      note.textContent = "If the embedded preview is not available yet, open Host in the system browser and continue sharing there.";
      setPreviewBadge(payload.serverRunning ? "Waiting For URL" : "Idle", payload.serverRunning ? "warn" : "idle");
      return;
    }

    if (previewUrl !== hostUrl) {
      previewUrl = hostUrl;
      previewLoaded = false;
      frame.src = hostUrl;
    }

    note.textContent = payload.hostReachable
      ? "The embedded preview is bound to the same Host URL that will be handed to the operator."
      : "The Host page can load locally, but LAN reachability still needs attention before handing off Viewer access.";

    if (!previewLoaded) {
      empty.hidden = false;
      title.textContent = "Loading Host Preview";
      body.textContent = "The embedded /host page is loading inside the HTML admin shell. If it stays blank, open Host in the browser once to verify the certificate prompt.";
      setPreviewBadge("Loading", "idle");
      return;
    }

    empty.hidden = true;
    if (hostState === "sharing") {
      setPreviewBadge("Sharing", "ok");
    } else if (hostState === "ready") {
      setPreviewBadge("Ready", "ok");
    } else if (hostState === "loading") {
      setPreviewBadge("Loading", "idle");
    } else if (payload.serverRunning) {
      setPreviewBadge("Attention", "warn");
    } else {
      setPreviewBadge("Idle", "idle");
    }
  }

  function switchTab(tab, notifyNative) {
    const normalizedTab = normalizeTab(tab);
    const changed = activeTab !== normalizedTab;
    activeTab = normalizedTab;
    document.querySelectorAll(".tab").forEach((button) => {
      button.classList.toggle("active", button.getAttribute("data-tab") === normalizedTab);
    });
    document.querySelectorAll(".view").forEach((view) => {
      view.classList.toggle("active", view.getAttribute("data-view") === normalizedTab);
    });
    if (notifyNative && changed) {
      send({ kind: "command", command: "switch-page", page: normalizedTab });
    }
  }

  function dashboardSuggestions(payload) {
    const items = [];
    if (!payload.serverRunning) {
      items.push(["Start the local service", "Use Start Sharing to launch the local HTTPS/WSS server before handing out links."]);
    }
    if (!payload.hostIp || payload.hostIp === "(not found)") {
      items.push(["Select a usable LAN address", "No primary host IP is selected yet. Refresh network detection or pick another adapter."]);
    }
    if (payload.serverRunning && !payload.healthReady) {
      items.push(["Investigate /health", "The process is running, but the local health probe is still not healthy."]);
    }
    if (payload.serverRunning && !payload.hostReachable) {
      items.push(["Check adapter selection", "The selected host address is not responding to the reachability probe."]);
    }
    if (!payload.certReady) {
      items.push([
        "Regenerate local certificates",
        payload.certDetail || "The local certificate is missing, expired, or does not match the current host entries."
      ]);
    }
    if (!payload.shareBundleExported) {
      items.push(["Refresh offline materials", "The share bundle has not been exported for this session yet."]);
    }
    if (items.length === 0) {
      items.push(["Ready for handoff", "The current session looks healthy enough for the operator to continue sharing."]);
    }
    return items;
  }

  function diagnosticsChecklist(payload) {
    return [
      ["Port listening", payload.canStartSharing ? "Ready or blocked by sharing state" : "Sharing active"],
      ["Local /health", payload.healthReady ? "OK" : "Needs attention"],
      ["Selected host IP", payload.hostReachable ? "Reachable" : "Not reachable yet"],
      ["Certificate", payload.certReady ? "Ready" : (payload.certDetail || "Not ready")],
      ["Bundle export", payload.shareBundleExported ? "Exported" : "Not exported"],
      ["WebView runtime", payload.webviewStatus || "Unknown"]
    ];
  }

  function sharingGuide(payload) {
    return [
      ["Same LAN", "Keep the viewer device on the same router or switch as the host and open the Viewer URL."],
      ["Hotspot mode", payload.hotspotRunning ? "Host hotspot is active. Join the SSID shown in Network, then open the Viewer URL." : "If no shared LAN is available, start hotspot in the Network tab first."],
      ["Certificate reminder", "First access may show a self-signed certificate prompt. Accept it for this local session."],
      ["Common failure", payload.hostReachable ? "If a viewer still fails, test the Viewer URL directly in a browser." : "If viewers fail, re-check the selected adapter and reachability first."]
    ];
  }

  function renderSuggestions(id, items) {
    $(id).innerHTML = items.map(([title, detail]) => {
      return "<article class=\"suggestion\"><h3>" + title + "</h3><p>" + detail + "</p></article>";
    }).join("");
  }

  function applySessionForm(payload) {
    if (sessionDirty) {
      return;
    }
    $("roomInput").value = payload.room || "";
    $("tokenInput").value = payload.token || "";
    $("bindInput").value = payload.bind || "";
    $("portInput").value = payload.port || 9443;
  }

  function applyHotspotForm(payload) {
    if (hotspotDirty) {
      return;
    }
    $("hotspotSsidInput").value = payload.hotspotSsid || "";
    $("hotspotPasswordInput").value = payload.hotspotPassword || "";
  }

  function renderCandidates(payload) {
    const root = $("adapterList");
    const candidates = payload.networkCandidates || [];
    if (!candidates.length) {
      root.innerHTML = "<div class=\"empty\">No adapter candidates were detected.</div>";
      return;
    }
    root.innerHTML = candidates.map((item, index) => {
      const flags = [];
      if (item.recommended) flags.push("Recommended");
      if (item.selected) flags.push("Selected");
      return "<article class=\"candidate\">" +
        "<div><h3>" + item.name + "</h3><p>" + item.ip + " | " + item.type + "</p><div class=\"chip-row\">" +
        flags.map((flag) => "<span class=\"chip\">" + flag + "</span>").join("") +
        "</div></div>" +
        "<button class=\"secondary\" data-command=\"select-adapter\" data-index=\"" + index + "\">Use As Main</button>" +
        "</article>";
    }).join("");
  }

  function renderMetrics(payload) {
    const metrics = [
      ["Rooms", payload.rooms],
      ["Viewers", payload.viewers],
      ["Host State", payload.hostState],
      ["/health", payload.healthReady ? "OK" : "ATTN"],
      ["Reachability", payload.hostReachable ? "OK" : "ATTN"]
    ];
    $("monitorMetrics").innerHTML = metrics.map(([label, value]) => {
      return "<article class=\"metric\"><span>" + label + "</span><strong>" + value + "</strong></article>";
    }).join("");
  }

  function renderChecklist(payload) {
    $("diagChecklist").innerHTML = diagnosticsChecklist(payload).map(([label, value]) => {
      return "<div class=\"checkitem\"><strong>" + label + "</strong><span>" + value + "</span></div>";
    }).join("");
  }

  function handoffSummary(payload) {
    const stateName = String(payload.handoffState || "").toLowerCase();
    const stateLabel = payload.handoffLabel || "Not started";
    const stateDetail = payload.handoffDetail || "Open Share Wizard or copy the Viewer URL when you are ready to hand off the session.";
    return [
      ["State", stateLabel],
      ["Share Wizard", payload.shareWizardOpened ? "Opened" : "Not opened yet"],
      ["Handoff started", payload.handoffStarted ? "Yes" : "No"],
      ["Viewer connected", payload.handoffDelivered ? "Yes" : "No"],
      ["Next step", stateName === "delivered" ? "Keep sharing or monitor the session." : stateDetail]
    ];
  }

  function quickFixItems(payload) {
    const items = [];
    if (!payload.serverRunning) {
      items.push(["Service is not running", "Start the local share service again before handing off access.", "quick-fix-sharing", "Start sharing"]);
    }
    if (!payload.hostIp || payload.hostIp === "(not found)" || !payload.hostReachable) {
      items.push(["LAN endpoint still needs attention", "Refresh adapter detection and re-check which address should be used as the main viewer entry.", "quick-fix-network", "Refresh network"]);
    }
    if (!payload.certReady) {
      items.push(["Certificate or trust path needs attention", payload.certDetail || "Open diagnostics and refresh the current local certificate / trust path.", "quick-fix-certificate", "Open diagnostics"]);
    }
    if ((payload.shareWizardOpened || payload.handoffStarted) && !payload.handoffDelivered && payload.serverRunning && payload.healthReady && payload.hostReachable && payload.certReady) {
      items.push(["Viewer handoff material is ready", "Copy the Viewer URL again or show the QR / share card while the other device connects.", "quick-fix-handoff", "Show QR + copy link"]);
    }
    if (!payload.hotspotRunning && (!payload.hostReachable || Number(payload.activeIpv4Candidates || 0) === 0)) {
      items.push(["Fallback path may be needed", "If the current LAN path is unstable, open or start hotspot before retrying the viewer handoff.", "quick-fix-hotspot", "Open hotspot path"]);
    }
    if (!items.length) {
      items.push(["No blocking issue detected", "The current session looks healthy. Open Share Wizard or keep monitoring viewer activity.", "show-share-wizard", "Open Share Wizard"]);
    }
    return items.slice(0, 4);
  }

  function renderQuickFixes(payload) {
    $("dashboardQuickFixes").innerHTML = quickFixItems(payload).map(([title, detail, command, label]) => {
      return '<article class="quick-fix"><div><h3>' + title + '</h3><p>' + detail + '</p></div><button class="secondary" data-command="' + command + '">' + label + '</button></article>';
    }).join("");
  }

  function render(payload) {
    Object.assign(state, payload || {});
    const nativeTab = normalizeTab(state.nativePage);
    if (nativeTab !== activeTab) {
      switchTab(nativeTab, false);
    }
    $("bridgeStatus").textContent = state.serverRunning ? "Bridge live / service running" : "Bridge live";
    $("bridgeStatus").className = "status " + (state.serverRunning ? "ok" : "idle");

    const detailLine = state.dashboardState === "ready"
      ? "Service and host page look ready for the next sharing session."
      : state.dashboardState === "sharing"
      ? "Sharing is active. Viewer link can be handed off right now."
      : state.dashboardState === "error"
      ? "The service is up, but live checks are still failing."
      : "The operator still needs to complete setup before sharing.";

    $("dashboardStateLabel").textContent = state.dashboardLabel || "Unknown";
    $("dashboardStateLabel").className = "status-label tone-" + (state.dashboardState || "not-ready");
    $("dashboardStateDetail").textContent = detailLine;

    setPairs("dashboardStatusGrid", [
      ["Host IP", state.hostIp],
      ["Port", state.port],
      ["Room", state.room],
      ["Viewers", state.viewers],
      ["Latest Error", state.dashboardError]
    ]);

    setPairs("dashboardNetworkCard", [
      ["Primary IPv4", state.hostIp],
      ["Network Mode", state.networkMode],
      ["Adapters", state.activeIpv4Candidates],
      ["Hotspot", state.hotspotStatus]
    ]);

    setPairs("dashboardServiceCard", [
      ["Server EXE", state.serverExePath],
      ["Bind + Port", (state.bind || "-") + ":" + (state.port || "-")],
      ["Cert Dir", state.certDir],
      ["Host State", state.hostState]
    ]);

    setPairs("dashboardSharingCard", [
      ["Viewer URL", state.viewerUrl],
      ["Copied", state.viewerUrlCopied],
      ["Bundle Exported", state.shareBundleExported],
      ["Bundle Dir", state.bundleDir]
    ]);

    setPairs("dashboardHealthCard", [
      ["/health", state.healthReady],
      ["Reachability", state.hostReachable],
      ["Heartbeat", state.recentHeartbeat],
      ["WebView", state.webviewStatus]
    ]);

    setPairs("dashboardHandoffCard", handoffSummary(state));
    renderQuickFixes(state);
    renderSuggestions("suggestions", dashboardSuggestions(state));

    applySessionForm(state);
    applyHotspotForm(state);
    renderCandidates(state);
    syncHotspotActions(state);

    setPairs("sessionSummary", [
      ["Host URL", state.hostUrl],
      ["Viewer URL", state.viewerUrl],
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir]
    ]);

    setPairs("sessionRuntimeCard", [
      ["Server Running", state.serverRunning],
      ["Rooms", state.rooms],
      ["Viewers", state.viewers],
      ["Recent Heartbeat", state.recentHeartbeat],
      ["Host Ready State", state.hostState],
      ["Local Reachability", state.localReachability]
    ]);
    updateHostPreview(state);

    setPairs("networkSummary", [
      ["Recommended IPv4", state.hostIp],
      ["Current Bind", state.bind],
      ["Reachability", state.localReachability],
      ["Wi-Fi Adapter", state.wifiAdapterPresent],
      ["Hotspot Supported", state.hotspotSupported],
      ["Current Hotspot State", state.hotspotStatus]
    ]);

    setPairs("wifiDirectCard", [
      ["Wi-Fi Direct", state.wifiDirectAvailable],
      ["Recommendation", state.wifiDirectAvailable ? "Use Connected Devices pairing in Windows." : "Use LAN or hotspot path."],
      ["Current Session Alias", state.room ? "LanShare-" + state.room : "LanShare-session"]
    ]);

    setPairs("hotspotCard", [
      ["Status", state.hotspotStatus],
      ["SSID", state.hotspotSsid],
      ["Password", state.hotspotPassword],
      ["Network Mode", state.networkMode]
    ]);

    setPairs("sharingAccessCard", [
      ["Host URL", state.hostUrl],
      ["Viewer URL", state.viewerUrl],
      ["Viewer Copied", state.viewerUrlCopied],
      ["Bundle Exported", state.shareBundleExported]
    ]);

    setPairs("sharingMaterialCard", [
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir],
      ["Server EXE", state.serverExePath],
      ["Current Warning", state.dashboardError]
    ]);

    $("sharingGuide").innerHTML = sharingGuide(state).map(([label, detail]) => {
      return "<article class=\"guide\"><h3>" + label + "</h3><p>" + detail + "</p></article>";
    }).join("");

    renderMetrics(state);
    $("timelineText").textContent = state.timelineText || "No timeline events yet.";
    $("logTailText").textContent = state.logTail || "No logs yet.";

    renderChecklist(state);
    renderSuggestions("diagActions", [
      ["Check subnet alignment", "Confirm the viewer device and the host still sit on the same local network."],
      ["Test URL directly", "If Viewer fails, paste the Viewer URL directly into a browser first."],
      ["Fallback to system hotspot", "If hotspot start fails, open the Windows hotspot settings and start it manually."]
    ]);
    setPairs("diagPaths", [
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir],
      ["Cert Dir", state.certDir],
      ["Server EXE", state.serverExePath]
    ]);
    $("diagWarningText").textContent = state.dashboardError || "None";

    setPairs("settingsGeneral", [
      ["Default Port", state.defaultPort],
      ["Default Bind", state.defaultBind],
      ["Room Rule", state.roomRule],
      ["Token Rule", state.tokenRule]
    ]);
    setPairs("settingsSharing", [
      ["Viewer Open Mode", state.defaultViewerOpenMode],
      ["Auto Copy Viewer Link", state.autoCopyViewerLink],
      ["Auto Generate QR", state.autoGenerateQr],
      ["Auto Export Bundle", state.autoExportBundle]
    ]);
    setPairs("settingsLogging", [
      ["Log Level", state.logLevel],
      ["Save stdout/stderr", state.saveStdStreams],
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir]
    ]);
    setPairs("settingsAdvanced", [
      ["Cert Bypass", state.certBypassPolicy],
      ["WebView Behavior", state.webViewBehavior],
      ["Startup Hook", state.startupHook],
      ["Current Native Page", state.nativePage]
    ]);

    $("rawState").textContent = JSON.stringify(payload || {}, null, 2);
  }

  function applySession() {
    sessionDirty = false;
    send({
      kind: "command",
      command: "apply-session",
      room: $("roomInput").value || "",
      token: $("tokenInput").value || "",
      bind: $("bindInput").value || "",
      port: Number($("portInput").value || 9443)
    });
  }

  function applyHotspot() {
    hotspotDirty = false;
    send({
      kind: "command",
      command: "apply-hotspot",
      ssid: $("hotspotSsidInput").value || "",
      password: $("hotspotPasswordInput").value || ""
    });
  }

  function syncHotspotActions(payload) {
    const startBtn = $("startHotspotBtn");
    const stopBtn = $("stopHotspotBtn");
    const applyBtn = $("applyHotspotBtn");
    const directControl = !!payload.hotspotSupported;

    applyBtn.disabled = false;
    stopBtn.disabled = !payload.hotspotRunning;

    if (directControl) {
      startBtn.disabled = !!payload.hotspotRunning;
      startBtn.textContent = "Start Hotspot";
      startBtn.title = "";
    } else {
      startBtn.disabled = false;
      startBtn.textContent = "Use System Hotspot";
      startBtn.title = "This machine does not support hostednetwork control. Open Windows hotspot settings instead.";
    }
  }

  function handleCommand(command, extra) {
    if (command === "request-snapshot") {
      send({ kind: "request-snapshot" });
      return;
    }
    if (command === "apply-session") {
      applySession();
      return;
    }
    if (command === "apply-hotspot") {
      applyHotspot();
      return;
    }
    if (command === "start-hotspot") {
      if (hotspotDirty) {
        applyHotspot();
      }
      if (!state.hotspotSupported && !state.hotspotRunning) {
        send({ kind: "command", command: "open-hotspot-settings" });
        return;
      }
    }
    send({ kind: "command", command, ...extra });
  }

  function handleLocalAction(action) {
    if (action === "reload-preview") {
      const frame = $("hostPreviewFrame");
      if (!state.serverRunning || !state.hostUrl) {
        handleCommand("request-snapshot");
        return;
      }
      previewLoaded = false;
      previewUrl = state.hostUrl;
      frame.src = state.hostUrl;
      updateHostPreview(state);
    }
  }

  function bindButtons() {
    document.addEventListener("click", (event) => {
      const button = event.target.closest("button");
      if (!button) return;

      if (button.hasAttribute("data-tab")) {
        switchTab(button.getAttribute("data-tab"), true);
        return;
      }
      if (button.hasAttribute("data-tab-target")) {
        switchTab(button.getAttribute("data-tab-target"), true);
        return;
      }

      if (button.hasAttribute("data-local-action")) {
        handleLocalAction(button.getAttribute("data-local-action"));
        return;
      }

      const command = button.getAttribute("data-command");
      if (!command) return;
      const index = button.getAttribute("data-index");
      handleCommand(command, index === null ? {} : { index: Number(index) });
    });

    ["roomInput", "tokenInput", "bindInput", "portInput"].forEach((id) => {
      $(id).addEventListener("input", () => {
        sessionDirty = true;
      });
    });

    ["hotspotSsidInput", "hotspotPasswordInput"].forEach((id) => {
      $(id).addEventListener("input", () => {
        hotspotDirty = true;
      });
    });

    $("hostPreviewFrame").addEventListener("load", () => {
      previewLoaded = true;
      updateHostPreview(state);
    });
  }

  function bindBridge() {
    if (!window.chrome || !window.chrome.webview) {
      $("bridgeStatus").textContent = "Running outside WebView2";
      return;
    }

    window.chrome.webview.addEventListener("message", (event) => {
      const message = event.data || {};
      if (message.name === "state.snapshot") {
        render(message.payload || {});
      }
    });

    send({ kind: "ready" });
  }

  bindButtons();
  switchTab(activeTab, false);
  bindBridge();
})();
