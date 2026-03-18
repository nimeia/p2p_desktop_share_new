(function () {
  const state = {};
  let activeTab = "dashboard";
  let currentRoute = "guide";
  let lastSimpleRoute = "guide";
  let sessionDirty = false;
  let hotspotDirty = false;
  let previewUrl = "";
  let previewLoaded = false;
  const hostBridge = {
    ready: false,
    requestSeq: 0,
    pendingCommand: null,
    inFlightRequestId: "",
    timeoutHandle: 0,
    lastBlockedReason: "",
    lastStatusKey: "",
  };

  const debugState = {
    entries: [],
    maxEntries: 120,
  };

  const guidedState = {
    initializedFromSnapshot: false,
    choiceMode: "",
    activeMode: "",
    startedAt: 0,
    lastCommandAt: 0,
    status: "idle",
    notice: "",
    issue: null,
    actions: createGuidedActions(),
    pendingHostOpen: false,
    hostWindowHint: "",
    justStopped: false,
  };

  function createGuidedActions() {
    return {
      refreshed: false,
      generated: false,
      selectedAdapter: false,
      hotspotAuto: false,
      hotspotStarted: false,
      serverStarted: false,
    };
  }

  function debugNow() {
    const now = new Date();
    const locale = window.LanShareI18n && typeof window.LanShareI18n.getLocale === "function"
      ? window.LanShareI18n.getLocale()
      : "en";
    return now.toLocaleTimeString(locale, { hour12: false });
  }

  function formatDebugExtra(extra) {
    if (extra === undefined || extra === null || extra === "") {
      return "";
    }
    if (typeof extra === "string") {
      return extra;
    }
    try {
      return JSON.stringify(extra);
    } catch {
      return String(extra);
    }
  }

  function debugLog(message, extra) {
    const suffix = formatDebugExtra(extra);
    const line = "[" + debugNow() + "] " + message + (suffix ? " | " + suffix : "");
    debugState.entries.push(line);
    if (debugState.entries.length > debugState.maxEntries) {
      debugState.entries.splice(0, debugState.entries.length - debugState.maxEntries);
    }
    const logNode = $("shareDebugLog");
    if (logNode) {
      logNode.textContent = debugState.entries.join("\n");
    }
    if (window.console && typeof window.console.log === "function") {
      window.console.log("[simple-share]", line);
    }
  }

  function $(id) {
    return document.getElementById(id);
  }

  function setText(id, text) {
    const node = $(id);
    if (node) {
      node.textContent = text;
    }
  }

  function setHtml(id, html) {
    const node = $(id);
    if (node) {
      node.innerHTML = html;
    }
  }

  function setStatusBadge(id, label, tone) {
    const node = $(id);
    if (!node) return;
    node.textContent = label;
    node.className = tone ? "status " + tone : "status";
  }

  function send(message) {
    if (!window.chrome || !window.chrome.webview || typeof window.chrome.webview.postMessage !== "function") {
      debugLog("native bridge unavailable", message && message.kind ? message.kind : "");
      setText("bridgeStatus", "Bridge unavailable");
      return;
    }
    if (message && (message.kind === "command" || message.kind === "request-snapshot" || message.kind === "ready")) {
      debugLog("send native", message);
    }
    window.chrome.webview.postMessage(JSON.stringify({ source: "admin-shell", ...message }));
  }

  function setPairs(id, entries) {
    const root = $(id);
    if (!root) return;
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

  function normalizeRoute(route) {
    if (route === "last-simple") {
      return lastSimpleRoute || "guide";
    }
    const value = String(route || "").toLowerCase();
    return ["guide", "prepare", "share", "advanced"].includes(value) ? value : "guide";
  }

  function switchRoute(route) {
    const normalized = normalizeRoute(route);
    currentRoute = normalized;
    if (normalized !== "advanced") {
      lastSimpleRoute = normalized;
    }
    document.querySelectorAll("[data-route-view]").forEach((node) => {
      const active = node.getAttribute("data-route-view") === normalized;
      node.hidden = !active;
    });
  }

  function requestRender() {
    render(state);
  }

  function currentPrepareMode() {
    if (guidedState.activeMode) return guidedState.activeMode;
    if (guidedState.choiceMode === "hotspot") return "hotspot";
    return "lan";
  }

  function hasUsableHostIp(payload) {
    const value = String(payload.hostIp || "");
    return !!value && value !== "(not found)" && value !== "0.0.0.0";
  }

  function hostStateValue(payload) {
    return String(payload.hostState || "").toLowerCase();
  }

  function captureStateValue(payload) {
    return String(payload.captureState || "").toLowerCase();
  }

  function captureLabelValue(payload) {
    return String(payload.captureLabel || "").trim();
  }

  function isCaptureSelecting(payload) {
    const value = captureStateValue(payload);
    return value === "selecting" || hostStateValue(payload) === "starting_share";
  }

  function hasActiveCapture(payload) {
    const value = captureStateValue(payload);
    return value === "active" || value === "sharing";
  }

  function isHostSharing(payload) {
    if (hasActiveCapture(payload)) {
      return true;
    }
    const value = hostStateValue(payload);
    return value === "sharing" || value === "shared" || value === "streaming";
  }

  function baseServiceReady(payload) {
    return !!payload.serverRunning && !!payload.healthReady;
  }

  function prepareReadyForMode(mode, payload) {
    if (mode === "hotspot") {
      return baseServiceReady(payload) && (!!payload.hotspotRunning || !!payload.hostReachable);
    }
    return baseServiceReady(payload) && !!payload.hostReachable;
  }

  function detectConnectionPath(payload) {
    if (payload.hotspotRunning) return "本机热点";
    if (String(payload.networkMode || "").toLowerCase() === "hotspot") return "本机热点";
    return "同一网络";
  }

  function yesNo(value, yesLabel, noLabel) {
    return value ? yesLabel : noLabel;
  }

  function makeIssue(key, title, detail, actionLabel, action) {
    return { key, title, detail, actionLabel, action };
  }

  function resetGuidedFlow(choiceMode) {
    guidedState.choiceMode = choiceMode || "";
    guidedState.activeMode = choiceMode === "hotspot" ? "hotspot" : "lan";
    guidedState.startedAt = Date.now();
    guidedState.lastCommandAt = 0;
    guidedState.status = choiceMode ? "working" : "idle";
    guidedState.notice = choiceMode === "auto"
      ? "系统先尝试同网共享，必要时会自动切换到本机热点。"
      : "";
    guidedState.issue = null;
    guidedState.actions = createGuidedActions();
    guidedState.pendingHostOpen = false;
    guidedState.hostWindowHint = "";
    guidedState.justStopped = false;
  }

  function startGuidedFlow(choiceMode) {
    resetGuidedFlow(choiceMode);
    switchRoute("prepare");
    requestRender();
  }

  function guidedModeLabel(mode) {
    if (mode === "hotspot") return "本机热点";
    if (mode === "auto") return "自动判断";
    return "同一网络";
  }

  function guidedActiveModeLabel() {
    const mode = currentPrepareMode();
    if (guidedState.choiceMode === "auto" && mode === "hotspot") {
      return "自动判断，当前已切换为本机热点";
    }
    if (guidedState.choiceMode === "auto") {
      return "自动判断，当前正在尝试同网共享";
    }
    return guidedModeLabel(mode);
  }

  function recommendedCandidateIndex(payload) {
    const candidates = payload.networkCandidates || [];
    if (!candidates.length) return -1;
    const recommended = candidates.findIndex((item) => item.recommended);
    if (recommended >= 0) return recommended;
    if (candidates.length === 1) return 0;
    return -1;
  }

  function canSendGuidedCommand() {
    return Date.now() - guidedState.lastCommandAt > 500;
  }

  function markGuidedCommandSent() {
    guidedState.lastCommandAt = Date.now();
  }

  function shouldFallbackToHotspot(payload, elapsedMs) {
    if (guidedState.choiceMode !== "auto") return false;
    if (currentPrepareMode() !== "lan") return false;
    if (payload.hotspotRunning) return false;
    if (elapsedMs < 2400) return false;

    const noLanPath = !hasUsableHostIp(payload) && Number(payload.activeIpv4Candidates || 0) === 0;
    const lanProbeFailed = payload.serverRunning && payload.healthReady && !payload.hostReachable && elapsedMs > 5200;
    return noLanPath || lanProbeFailed;
  }

  function activateHotspotFallback() {
    guidedState.activeMode = "hotspot";
    guidedState.notice = "未检测到稳定的同网路径，正在改用本机热点。";
    guidedState.issue = null;
    guidedState.status = "working";
    guidedState.actions.hotspotAuto = false;
    guidedState.actions.hotspotStarted = false;
    guidedState.lastCommandAt = 0;
    requestRender();
  }

  function computeGuidedIssue(payload, mode, elapsedMs) {
    if (mode === "lan" && elapsedMs > 5200 && !hasUsableHostIp(payload) && Number(payload.activeIpv4Candidates || 0) === 0) {
      return makeIssue(
        "no-network",
        "未检测到可用网络",
        "当前没有找到适合同网共享的地址。请确认电脑已连到网络，或者改用本机热点。",
        "改用本机热点",
        "switch-hotspot"
      );
    }

    if (mode === "lan" && payload.serverRunning && elapsedMs > 7800 && !payload.hostReachable) {
      return makeIssue(
        "lan-path",
        "当前同网路径还不可用",
        payload.remoteViewerDetail || "系统已经启动共享服务，但当前局域网地址还不能稳定用于交付。",
        "重新检测网络",
        "refresh-network"
      );
    }

    if (mode === "hotspot" && guidedState.actions.hotspotStarted && elapsedMs > 7600 && !payload.hotspotRunning) {
      if (!payload.hotspotSupported || String(payload.hotspotStatus || "").toLowerCase().includes("system settings required")) {
        return makeIssue(
          "hotspot-settings",
          "当前机器无法直接启动热点",
          "这台电脑需要通过 Windows 系统设置来开启热点。",
          "打开系统热点设置",
          "open-hotspot-settings"
        );
      }
      return makeIssue(
        "hotspot-start",
        "热点启动失败",
        "系统尝试启动热点但没有成功。你可以重试，或者改用系统热点设置继续。",
        "重试启动热点",
        "retry-hotspot"
      );
    }

    if (guidedState.actions.serverStarted && elapsedMs > 8200 && !payload.serverRunning) {
      return makeIssue(
        "service-start",
        "共享服务启动失败",
        payload.lastError || payload.dashboardError || "系统未能启动本地共享服务。",
        "重新启动服务",
        "retry-service"
      );
    }

    if (payload.serverRunning && elapsedMs > 8200 && !payload.healthReady) {
      return makeIssue(
        "service-health",
        "共享服务还未准备好",
        payload.dashboardError || "服务已经启动，但健康检查还没有通过。",
        "刷新状态",
        "request-snapshot"
      );
    }

    if (false && payload.serverRunning && elapsedMs > 8200 && !payload.certReady) {
      return makeIssue(
        "certificate",
        "本地安全准备未完成",
        payload.certDetail || "当前共享地址对应的本地证书还没有准备好。",
        "处理证书",
        "trust-local-certificate"
      );
    }

    return null;
  }

  function runGuidedPreparation(payload) {
    if (currentRoute !== "prepare" || !guidedState.choiceMode) return;

    const elapsedMs = Date.now() - guidedState.startedAt;
    const mode = currentPrepareMode();

    if (shouldFallbackToHotspot(payload, elapsedMs)) {
      activateHotspotFallback();
      return;
    }

    if (prepareReadyForMode(mode, payload)) {
      guidedState.status = "ready";
      guidedState.issue = null;
      switchRoute("share");
      requestRender();
      return;
    }

    const issue = computeGuidedIssue(payload, mode, elapsedMs);
    if (issue) {
      guidedState.status = "blocked";
      guidedState.issue = issue;
      return;
    }

    guidedState.status = "working";
    guidedState.issue = null;
    if (!canSendGuidedCommand()) return;

    if (!guidedState.actions.refreshed) {
      handleCommand("refresh-network");
      guidedState.actions.refreshed = true;
      markGuidedCommandSent();
      return;
    }

    if (!guidedState.actions.generated) {
      handleCommand("generate-room-token");
      guidedState.actions.generated = true;
      markGuidedCommandSent();
      return;
    }

    const preferredIndex = recommendedCandidateIndex(payload);
    if (preferredIndex >= 0 &&
        payload.networkCandidates &&
        payload.networkCandidates[preferredIndex] &&
        !payload.networkCandidates[preferredIndex].selected &&
        !guidedState.actions.selectedAdapter) {
      handleCommand("select-adapter", { index: preferredIndex });
      guidedState.actions.selectedAdapter = true;
      markGuidedCommandSent();
      return;
    }

    if (mode === "hotspot" && !guidedState.actions.hotspotAuto) {
      handleCommand("auto-hotspot");
      guidedState.actions.hotspotAuto = true;
      markGuidedCommandSent();
      return;
    }

    if (mode === "hotspot" && !payload.hotspotRunning && !guidedState.actions.hotspotStarted) {
      handleCommand("start-hotspot");
      guidedState.actions.hotspotStarted = true;
      markGuidedCommandSent();
      return;
    }

    if (!payload.serverRunning && !guidedState.actions.serverStarted) {
      handleCommand("start-server");
      guidedState.actions.serverStarted = true;
      markGuidedCommandSent();
    }
  }

  function setPreviewBadge(label, tone) {
    const badge = $("hostPreviewBadge");
    if (!badge) return;
    badge.textContent = label;
    badge.className = "status status-inline " + tone;
  }

  function updateHostPreview(payload) {
    const frame = $("hostPreviewFrame");
    const empty = $("hostPreviewEmpty");
    const title = $("hostPreviewEmptyTitle");
    const body = $("hostPreviewEmptyBody");
    const note = $("hostPreviewNote");
    if (!frame || !empty || !title || !body || !note) return;

    const hostState = String(payload.hostState || "").toLowerCase();
    const hostUrl = payload.serverRunning ? String(payload.hostUrl || "") : "";

    if (!hostUrl) {
      if (previewUrl) {
        frame.src = "about:blank";
      }
      previewUrl = "";
      previewLoaded = false;
      hostBridge.ready = false;
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
      hostBridge.ready = false;
      frame.src = hostUrl;
    }

    note.textContent = payload.hostReachable
      ? "The embedded preview is bound to the same Host URL that will be handed to the operator."
      : "The Host page can load locally, but LAN reachability still needs attention before handing off Viewer access.";

    if (!previewLoaded) {
      empty.hidden = false;
      title.textContent = "Loading Host Preview";
      body.textContent = "The embedded /host page is loading inside the local admin shell. If it stays blank, wait a moment and refresh the current snapshot.";
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

  function nextHostBridgeRequestId() {
    hostBridge.requestSeq += 1;
    return "host-bridge-" + hostBridge.requestSeq;
  }

  function clearHostBridgeTimeout() {
    if (hostBridge.timeoutHandle) {
      window.clearTimeout(hostBridge.timeoutHandle);
      hostBridge.timeoutHandle = 0;
    }
  }

  function mergeHostBridgePatch(patch) {
    if (!patch || typeof patch !== "object") return;
    Object.assign(state, patch);
    syncGuidedShareFlags(state);
    renderSimpleMode(state);
    renderAdvancedShell(state);
  }

  function hostBridgeRequestMatches(requestId) {
    return !!hostBridge.pendingCommand && hostBridge.pendingCommand.requestId === requestId;
  }

  function finalizeHostBridgeRequest(requestId) {
    if (hostBridge.inFlightRequestId === requestId) {
      hostBridge.inFlightRequestId = "";
    }
    if (hostBridgeRequestMatches(requestId)) {
      hostBridge.pendingCommand = null;
    }
    clearHostBridgeTimeout();
  }

  function hasPendingHostBridgeCommand() {
    return !!hostBridge.pendingCommand || !!hostBridge.inFlightRequestId;
  }

  function fallbackOpenHostWindow(command) {
    guidedState.pendingHostOpen = false;
    if (command === "choose-share") {
      guidedState.hostWindowHint = "嵌入共享页暂时没有响应，已切换为独立共享窗口。请在打开的共享窗口里选择要共享的窗口或屏幕。";
    } else {
      guidedState.hostWindowHint = "嵌入共享页暂时没有响应，已切换为独立共享窗口。请在打开的共享窗口里点击 Start Sharing 并选择要共享的内容。";
    }
    requestRender();
  }

  function handleQueuedHostBridgeTimeout(requestId) {
    if (hostBridge.inFlightRequestId === requestId) {
      return;
    }
    const pending = hostBridge.pendingCommand;
    if (!pending || pending.requestId !== requestId) {
      return;
    }
    debugLog("queued host bridge timeout", { requestId, command: pending.command });
    hostBridge.pendingCommand = null;
    clearHostBridgeTimeout();
    if (pending.stopServerAfter) {
      guidedState.pendingHostOpen = false;
      guidedState.hostWindowHint = "共享页未响应，已直接结束本次共享。";
      handleCommand("stop-server");
      requestRender();
      return;
    }
    fallbackOpenHostWindow(pending.command);
  }

  function handleHostBridgeTimeout(requestId) {
    if (hostBridge.inFlightRequestId !== requestId) return;
    const pending = hostBridge.pendingCommand;
    hostBridge.inFlightRequestId = "";
    clearHostBridgeTimeout();
    if (!pending || pending.requestId !== requestId) {
      return;
    }
    debugLog("in-flight host bridge timeout", { requestId, command: pending.command });
    hostBridge.pendingCommand = null;
    if (pending.stopServerAfter) {
      guidedState.pendingHostOpen = false;
      guidedState.hostWindowHint = "正在结束本次共享。";
      handleCommand("stop-server");
      requestRender();
      return;
    }
    fallbackOpenHostWindow(pending.command);
  }

  function tryFlushHostBridgeCommand() {
    const pending = hostBridge.pendingCommand;
    if (!pending || hostBridge.inFlightRequestId) return false;

    const frame = $("hostPreviewFrame");
    let blockedReason = "";
    if (!frame || !frame.contentWindow) {
      blockedReason = "frame-missing";
    } else if (!previewLoaded) {
      blockedReason = "preview-not-loaded";
    } else if (!hostBridge.ready) {
      blockedReason = "host-bridge-not-ready";
    } else if (!state.hostUrl) {
      blockedReason = "host-url-empty";
    }
    if (blockedReason) {
      if (hostBridge.lastBlockedReason !== blockedReason) {
        hostBridge.lastBlockedReason = blockedReason;
        debugLog("host bridge blocked", {
          blockedReason,
          requestId: pending.requestId,
          command: pending.command,
          previewLoaded,
          hostReady: hostBridge.ready,
          hostUrl: state.hostUrl || "",
        });
      }
      return false;
    }
    hostBridge.lastBlockedReason = "";

    hostBridge.inFlightRequestId = pending.requestId;
    clearHostBridgeTimeout();
    debugLog("flush host bridge command", { requestId: pending.requestId, command: pending.command });
    hostBridge.timeoutHandle = window.setTimeout(() => {
      handleHostBridgeTimeout(pending.requestId);
    }, pending.timeoutMs || 9000);

    frame.contentWindow.postMessage({
      source: "lan-share-admin",
      kind: "host-control",
      requestId: pending.requestId,
      command: pending.command,
    }, "*");
    return true;
  }

  function queueHostBridgeCommand(command, options) {
    if (hostBridge.pendingCommand || hostBridge.inFlightRequestId) {
      debugLog("host bridge already busy", {
        command,
        pendingCommand: hostBridge.pendingCommand ? hostBridge.pendingCommand.command : "",
        inFlightRequestId: hostBridge.inFlightRequestId,
      });
      return hostBridge.pendingCommand ? hostBridge.pendingCommand.requestId : hostBridge.inFlightRequestId;
    }
    const pending = {
      requestId: nextHostBridgeRequestId(),
      command,
      stopServerAfter: !!(options && options.stopServerAfter),
      timeoutMs: options && options.timeoutMs ? options.timeoutMs : 9000,
      queueTimeoutMs: options && options.queueTimeoutMs ? options.queueTimeoutMs : (state.serverRunning ? 4500 : 12000),
    };
    hostBridge.pendingCommand = pending;
    guidedState.pendingHostOpen = true;
    debugLog("queue host bridge command", pending);
    clearHostBridgeTimeout();
    hostBridge.timeoutHandle = window.setTimeout(() => {
      handleQueuedHostBridgeTimeout(pending.requestId);
    }, pending.queueTimeoutMs);
    tryFlushHostBridgeCommand();
    return pending.requestId;
  }

  function startGuidedShareCommand(command) {
    debugLog("startGuidedShareCommand", {
      command,
      serverRunning: state.serverRunning,
      hostUrl: state.hostUrl || "",
      previewLoaded,
      hostReady: hostBridge.ready,
    });
    guidedState.justStopped = false;
    const starting = command === "choose-share"
      ? "正在打开共享选择，请在系统弹窗中选择窗口或屏幕。"
      : "正在开始共享，请在系统弹窗中选择窗口或屏幕。";
    const waiting = state.serverRunning
      ? starting
      : "系统正在准备共享环境，准备完成后会自动打开共享选择。";
    guidedState.hostWindowHint = waiting;

    if (!state.serverRunning) {
      handleCommand("start-server");
    }

    queueHostBridgeCommand(command);
    switchRoute("share");
    requestRender();
  }

  function stopGuidedShareCommand() {
    debugLog("stopGuidedShareCommand", {
      serverRunning: state.serverRunning,
      hostState: state.hostState || "",
      captureState: state.captureState || "",
    });
    guidedState.pendingHostOpen = false;
    guidedState.justStopped = true;
    guidedState.hostWindowHint = "正在结束本次共享。";

    if (isHostSharing(state) || isCaptureSelecting(state)) {
      queueHostBridgeCommand("stop-share", { stopServerAfter: true, timeoutMs: 5000 });
    } else {
      handleCommand("stop-server");
    }
    requestRender();
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
      items.push(["Start the local service", "Use Start Sharing to launch the local HTTP/WS service before handing out links."]);
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
    if (payload.serverRunning && !payload.firewallReady) {
      items.push(["Review inbound firewall policy", payload.firewallDetail || "Windows Firewall does not yet show a clear inbound allow path for the current viewer entry."]);
    }
    if (payload.serverRunning && !payload.remoteViewerReady) {
      items.push(["Validate remote viewer reachability", payload.remoteViewerDetail || "The current Viewer URL is not yet validated for another device on the LAN."]);
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
      ["Firewall inbound path", payload.firewallReady ? "Ready" : (payload.firewallDetail || "Needs attention")],
      ["Remote viewer path", payload.remoteViewerReady ? "Ready" : (payload.remoteViewerDetail || "Needs attention")],
      ["Bundle export", payload.shareBundleExported ? "Exported" : "Not exported"],
      ["WebView runtime", payload.webviewStatus || "Unknown"]
    ];
  }

  function sharingGuide(payload) {
    return [
      ["Same LAN", "Keep the viewer device on the same router or switch as the host and open the Viewer URL."],
      ["Hotspot mode", payload.hotspotRunning ? "Host hotspot is active. Join the SSID shown in Network, then open the Viewer URL." : "If no shared LAN is available, start hotspot in the Network tab first."],
      ["Local access", "The local admin and host pages now run over plain HTTP on this machine. The viewer only needs the LAN Viewer URL."],
      ["Firewall", payload.firewallReady ? "Firewall looks compatible with inbound viewer traffic on this machine." : (payload.firewallDetail || "Open Windows Firewall settings and confirm there is an inbound allow rule for the current server path or port.")],
      ["Common failure", payload.remoteViewerReady ? "If a viewer still fails, test the Viewer URL directly in a browser." : (payload.remoteViewerDetail || "If viewers fail, re-check adapter selection, firewall policy, and same-LAN reachability first.")]
    ];
  }

  function renderSuggestions(id, items) {
    setHtml(id, items.map(([title, detail]) => {
      return "<article class=\"suggestion\"><h3>" + title + "</h3><p>" + detail + "</p></article>";
    }).join(""));
  }

  function applySessionForm(payload) {
    if (sessionDirty) {
      return;
    }
    if ($("roomInput")) $("roomInput").value = payload.room || "";
    if ($("tokenInput")) $("tokenInput").value = payload.token || "";
    if ($("bindInput")) $("bindInput").value = payload.bind || "";
    if ($("portInput")) $("portInput").value = payload.port || 9443;
  }

  function applyHotspotForm(payload) {
    if (hotspotDirty) {
      return;
    }
    if ($("hotspotSsidInput")) $("hotspotSsidInput").value = payload.hotspotSsid || "";
    if ($("hotspotPasswordInput")) $("hotspotPasswordInput").value = payload.hotspotPassword || "";
  }

  function renderCandidates(payload) {
    const root = $("adapterList");
    if (!root) return;
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
    setHtml("monitorMetrics", metrics.map(([label, value]) => {
      return "<article class=\"metric\"><span>" + label + "</span><strong>" + value + "</strong></article>";
    }).join(""));
  }

  function renderChecklist(payload) {
    setHtml("diagChecklist", diagnosticsChecklist(payload).map(([label, value]) => {
      return "<div class=\"checkitem\"><strong>" + label + "</strong><span>" + value + "</span></div>";
    }).join(""));
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
    if (payload.webviewStatus === "runtime-unavailable" || payload.webviewStatus === "controller-unavailable") {
      items.push(["Embedded admin preview needs WebView2 runtime attention", "Run the runtime helper or install/repair Evergreen WebView2 Runtime, then reopen the admin shell.", "check-webview-runtime", "Check WebView2 runtime"]);
    }
    if (payload.serverRunning && !payload.firewallReady) {
      items.push(["Firewall inbound path still needs attention", payload.firewallDetail || "Open Windows Firewall settings and confirm an inbound allow rule exists for the current share path.", "open-firewall-settings", "Open firewall settings"]);
    }
    if (payload.serverRunning && !payload.remoteViewerReady) {
      items.push(["Remote viewer path still needs validation", payload.remoteViewerDetail || "Collect a local network diagnostics report before retrying from another device.", "run-network-diagnostics", "Run diagnostics"]);
      if (payload.remoteProbeAction) {
        items.push(["Prepare a remote-device test guide", payload.remoteProbeAction, "export-remote-probe-guide", "Export guide"]);
      }
    }
    if ((payload.shareWizardOpened || payload.handoffStarted) && !payload.handoffDelivered && payload.serverRunning && payload.healthReady && payload.hostReachable) {
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
    setHtml("dashboardQuickFixes", quickFixItems(payload).map(([title, detail, command, label]) => {
      return "<article class=\"quick-fix\"><div><h3>" + title + "</h3><p>" + detail + "</p></div><button class=\"secondary\" data-command=\"" + command + "\">" + label + "</button></article>";
    }).join(""));
  }

  function updateRouteFromSnapshot(payload) {
    if (guidedState.initializedFromSnapshot) return;
    if (!Object.prototype.hasOwnProperty.call(payload || {}, "serverRunning")) return;
    if (payload.serverRunning || isHostSharing(payload) || Number(payload.viewers || 0) > 0) {
      switchRoute("share");
    } else {
      switchRoute("guide");
    }
    guidedState.initializedFromSnapshot = true;
  }

  function syncGuidedShareFlags(payload) {
    if (isCaptureSelecting(payload)) {
      guidedState.pendingHostOpen = true;
      guidedState.justStopped = false;
      guidedState.hostWindowHint = "共享选择器已经打开，请在系统弹窗中选择要共享的窗口或屏幕。";
      return;
    }

    if (isHostSharing(payload)) {
      guidedState.pendingHostOpen = false;
      guidedState.justStopped = false;
      guidedState.hostWindowHint = Number(payload.viewers || 0) > 0
        ? "接收方已经连接。你可以继续共享，也可以停止本次共享。"
        : "共享已经开始，等待接收方连接。";
      return;
    }

    if (!payload.serverRunning && guidedState.justStopped) {
      guidedState.hostWindowHint = "共享已停止。你可以重新开始，或返回上一步更换连接方式。";
      return;
    }

    if (!payload.serverRunning && !guidedState.pendingHostOpen) {
      guidedState.hostWindowHint = "";
    }
  }

  function renderGuide(payload) {
    const guideBadge = payload.serverRunning
      ? "共享服务可继续"
      : payload.hotspotRunning
      ? "热点已就绪"
      : hasUsableHostIp(payload)
      ? "已检测到网络"
      : "等待检测";
    const guideTone = payload.serverRunning || payload.hotspotRunning || hasUsableHostIp(payload) ? "ok" : "idle";
    setStatusBadge("guideBridgeBadge", guideBadge, guideTone);

    setPairs("guideEnvironmentCard", [
      ["当前路径", detectConnectionPath(payload)],
      ["共享地址", payload.hostIp || "(未检测到)"],
      ["热点控制", payload.hotspotSupported ? "可直接启动" : "需系统设置"],
      ["热点状态", payload.hotspotStatus || "stopped"]
    ]);

    const sessionState = isHostSharing(payload)
      ? "正在共享"
      : payload.serverRunning
      ? "服务已启动"
      : "尚未开始";

    setPairs("guideSessionCard", [
      ["当前状态", sessionState],
      ["连接人数", payload.viewers],
      ["访问地址", payload.viewerUrl || "准备后自动生成"],
      ["最近交付", payload.handoffLabel || "未开始"]
    ]);

    const resumeBtn = $("guideResumeBtn");
    if (resumeBtn) {
      resumeBtn.disabled = !(payload.serverRunning || isHostSharing(payload) || Number(payload.viewers || 0) > 0);
    }
  }

  function buildPrepareSteps(payload) {
    const mode = currentPrepareMode();
    const ready = prepareReadyForMode(mode, payload);

    if (mode === "hotspot") {
      return [
        {
          label: "正在生成热点信息",
          detail: "系统正在准备热点名称和密码。",
          complete: !!payload.hotspotSsid && !!payload.hotspotPassword,
        },
        {
          label: "正在启动热点",
          detail: "接收方需要先连接这个热点。",
          complete: !!payload.hotspotRunning,
        },
        {
          label: "正在启动共享服务",
          detail: "本地共享服务启动后才能生成访问入口。",
          complete: !!payload.serverRunning,
        },
        {
          label: "正在生成连接信息",
          detail: "系统会准备访问地址与二维码。",
          complete: !!payload.viewerUrl,
        },
        {
          label: "准备完成",
          detail: "可以进入共享页面，选择要共享的内容。",
          complete: ready,
        }
      ];
    }

    return [
      {
        label: "正在检测网络",
        detail: "系统正在确认当前电脑的可用网络路径。",
        complete: hasUsableHostIp(payload) || Number(payload.activeIpv4Candidates || 0) > 0,
      },
      {
        label: "正在选择共享地址",
        detail: "如有多个地址，系统会优先选推荐的那个。",
        complete: hasUsableHostIp(payload),
      },
      {
        label: "正在启动共享服务",
        detail: "本地共享服务启动后才能生成访问入口。",
        complete: !!payload.serverRunning,
      },
      {
        label: "正在生成连接信息",
        detail: "系统会准备访问地址与二维码。",
        complete: !!payload.viewerUrl,
      },
      {
        label: "准备完成",
        detail: "可以进入共享页面，选择要共享的内容。",
        complete: ready,
      }
    ];
  }

  function renderPrepareSteps(payload) {
    const root = $("prepareSteps");
    if (!root) return;
    const items = buildPrepareSteps(payload);
    const blocked = guidedState.status === "blocked";
    const currentIndex = items.findIndex((item) => !item.complete);

    root.innerHTML = items.map((item, index) => {
      const classes = ["step-item"];
      if (item.complete) {
        classes.push("is-complete");
      } else if (blocked && index === currentIndex) {
        classes.push("is-blocked");
      } else if (index === currentIndex) {
        classes.push("is-current");
      }
      return "<div class=\"" + classes.join(" ") + "\">" +
        "<div class=\"step-index\">" + (item.complete ? "OK" : String(index + 1)) + "</div>" +
        "<div class=\"step-copy\"><strong>" + item.label + "</strong><span>" + item.detail + "</span></div>" +
        "</div>";
    }).join("");
  }

  function renderPrepare(payload) {
    const mode = currentPrepareMode();
    const ready = prepareReadyForMode(mode, payload);
    const title = ready
      ? "准备完成，即将进入共享页面"
      : guidedState.status === "blocked"
      ? "当前准备过程被阻塞"
      : mode === "hotspot"
      ? "正在准备本机热点与共享服务"
      : "正在准备同网共享环境";

    const detail = ready
      ? "系统已经准备好当前共享环境。你可以开始选择共享内容了。"
      : guidedState.issue
      ? guidedState.issue.detail
      : mode === "hotspot"
      ? "请稍候，系统会先准备热点信息，再启动本地共享服务。"
      : "请稍候，系统会自动选择共享地址，并启动本地共享服务。";

    setText("prepareModeBadgeText", guidedActiveModeLabel());
    setText("prepareTitle", title);
    setText("prepareDetail", detail);
    setText("prepareModeNote", guidedState.notice || "系统会尽量减少手动操作，只在确实需要你介入时才停下来。");

    if (guidedState.status === "blocked") {
      setStatusBadge("prepareBadge", "需要处理", "warn");
    } else if (ready) {
      setStatusBadge("prepareBadge", "已准备", "ok");
    } else {
      setStatusBadge("prepareBadge", "处理中", "idle");
    }

    renderPrepareSteps(payload);

    setPairs("prepareConnectionCard", [
      ["当前路径", guidedActiveModeLabel()],
      ["共享地址", payload.hostIp || "(等待检测)"],
      ["热点名称", payload.hotspotSsid || "(未使用)"],
      ["热点状态", payload.hotspotStatus || "stopped"]
    ]);

    setPairs("prepareReadinessCard", [
      ["共享服务", yesNo(payload.serverRunning, "已启动", "未启动")],
      ["本地健康", yesNo(payload.healthReady, "通过", "未通过")],
      ["访问地址", payload.viewerUrl || "(生成中)"],
      ["本机控制页", payload.hostUrl || "(生成中)"]
    ]);

    const issueCard = $("prepareIssueCard");
    if (issueCard) {
      issueCard.hidden = !guidedState.issue;
    }
    setText("prepareIssueTitle", guidedState.issue ? guidedState.issue.title : "需要处理的问题");
    setText("prepareIssueTag", guidedState.issue ? "阻塞" : "状态");
    setText("prepareIssueText", guidedState.issue ? guidedState.issue.detail : "当前没有阻塞问题。");

    const primaryBtn = $("preparePrimaryBtn");
    if (primaryBtn) {
      if (guidedState.issue) {
        primaryBtn.textContent = guidedState.issue.actionLabel;
        primaryBtn.dataset.guidedAction = guidedState.issue.action;
      } else if (ready) {
        primaryBtn.textContent = "进入共享页面";
        primaryBtn.dataset.guidedAction = "resume-share";
      } else {
        primaryBtn.textContent = "刷新状态";
        primaryBtn.dataset.guidedAction = "request-snapshot";
      }
    }

    const secondaryBtn = $("prepareSecondaryBtn");
    if (secondaryBtn) {
      secondaryBtn.textContent = "返回选择";
      secondaryBtn.dataset.guidedAction = "go-guide";
    }
  }

  function buildShareStatus(payload) {
    if (isCaptureSelecting(payload)) {
      return {
        title: "正在等待你选择共享内容",
        detail: guidedState.hostWindowHint || "系统共享选择器已经打开，请选择要共享的窗口或屏幕。",
        badge: "选择中",
        tone: "warn",
      };
    }

    if (isHostSharing(payload) && Number(payload.viewers || 0) > 0) {
      return {
        title: "已有设备连接",
        detail: "共享已经开始，对方设备已经连入。你可以继续共享，或者结束本次共享。",
        badge: "已连接",
        tone: "ok",
      };
    }

    if (isHostSharing(payload)) {
      return {
        title: "正在共享",
        detail: "共享已经开始，正在等待对方连接。你可以让对方扫码或打开地址进入。",
        badge: "共享中",
        tone: "ok",
      };
    }

    if (guidedState.justStopped && !payload.serverRunning) {
      return {
        title: "共享已停止",
        detail: "本次共享已经结束。你可以重新开始，或返回引导重新选择连接方式。",
        badge: "已停止",
        tone: "warn",
      };
    }

    if (baseServiceReady(payload)) {
      return {
        title: "等待选择共享内容",
        detail: guidedState.hostWindowHint || "点击“开始共享”或“选择共享窗口”，然后在打开的共享窗口中选择要共享的内容。",
        badge: "待开始",
        tone: "idle",
      };
    }

    if (payload.serverRunning) {
      return {
        title: "共享前还差一步",
        detail: payload.dashboardError || "共享服务已启动，但还有一项准备尚未完成。",
        badge: "需修复",
        tone: "warn",
      };
    }

    return {
      title: "请先准备共享环境",
      detail: "返回引导页，系统会先帮你准备网络路径和共享服务，再进入这里选择共享内容。",
      badge: "未准备",
      tone: "idle",
    };
  }

  function buildShareAttention(payload) {
    if (!payload.serverRunning) {
      return {
        title: "共享服务尚未准备好",
        text: "请先返回引导页准备共享环境，再回来开始共享内容。",
        actionLabel: "返回引导",
        action: "go-guide",
      };
    }

    if (false && !payload.certReady) {
      return {
        title: "本地安全准备未完成",
        text: payload.certDetail || "当前共享地址对应的本地证书还没有准备好。",
        actionLabel: "处理证书",
        action: "trust-local-certificate",
      };
    }

    if (!payload.hostReachable && !payload.hotspotRunning) {
      return {
        title: "当前共享地址还不可用",
        text: payload.remoteViewerDetail || "当前网络地址还不能稳定用于共享，建议重新检测网络。",
        actionLabel: "重新检测网络",
        action: "refresh-network",
      };
    }

    if (payload.serverRunning && !payload.firewallReady) {
      return {
        title: "共享前还差一步",
        text: payload.firewallDetail || "Windows 防火墙可能会阻止其它设备访问当前共享地址。",
        actionLabel: "打开防火墙设置",
        action: "open-firewall-settings",
      };
    }

    if (payload.serverRunning && !payload.remoteViewerReady) {
      return {
        title: "建议先验证访问路径",
        text: payload.remoteViewerDetail || "当前访问路径还没有完全验证，建议先运行一次网络诊断。",
        actionLabel: "运行网络诊断",
        action: "run-network-diagnostics",
      };
    }

    return null;
  }

  function renderShare(payload) {
    const status = buildShareStatus(payload);
    const attention = buildShareAttention(payload);
    const captureLabel = captureLabelValue(payload);
    const selecting = isCaptureSelecting(payload);

    setText("shareTitle", status.title);
    setText("shareDetail", status.detail);
    setStatusBadge("shareBadge", status.badge, status.tone);
    setText(
      "shareHelperText",
      guidedState.hostWindowHint ||
      (payload.hotspotRunning
        ? "请让接收方先连接热点，再扫码或打开地址。共享内容需要在打开的共享窗口中选择。"
        : "你可以先把地址交给对方，再开始共享窗口或屏幕。")
    );

    const shareAttentionCard = $("shareAttentionCard");
    if (shareAttentionCard) {
      shareAttentionCard.hidden = !attention;
    }
    setText("shareAttentionTitle", attention ? attention.title : "共享前还差一步");
    setText("shareAttentionText", attention ? attention.text : "当前没有额外提醒。");

    const shareAttentionBtn = $("shareAttentionBtn");
    if (shareAttentionBtn) {
      shareAttentionBtn.dataset.guidedAction = attention ? attention.action : "request-snapshot";
      shareAttentionBtn.textContent = attention ? attention.actionLabel : "刷新状态";
    }

    setPairs("shareAccessCard", [
      ["访问地址", payload.viewerUrl || "(准备后自动生成)"],
      ["二维码", payload.viewerUrl ? "点击“显示二维码”打开" : "准备后自动生成"],
      ["热点名称", payload.hotspotRunning ? (payload.hotspotSsid || "(未命名)") : "未使用"],
      ["热点密码", payload.hotspotRunning ? (payload.hotspotPassword || "(未生成)") : "未使用"]
    ]);

    setPairs("shareMethodCard", [
      ["当前路径", detectConnectionPath(payload)],
      ["共享地址", payload.hostIp || "(等待检测)"],
      ["热点状态", payload.hotspotStatus || "stopped"],
      ["本地健康", yesNo(payload.healthReady, "正常", "需注意")]
    ]);

    setPairs("shareSessionCard", [
      ["当前状态", isHostSharing(payload) ? "正在共享" : (payload.serverRunning ? "等待选择内容" : "未启动")],
      ["连接人数", payload.viewers],
      ["交付状态", payload.handoffLabel || "未开始"],
      ["最近提示", payload.handoffDetail || "准备完成后会提示下一步"]
    ]);

    const nextSteps = [];
    if (!isHostSharing(payload)) {
      nextSteps.push(["开始共享内容", "点击上方的“开始共享”，系统会打开共享窗口；随后在共享窗口里点击 Start Sharing 并选择窗口或屏幕。"]);
    } else if (Number(payload.viewers || 0) === 0) {
      nextSteps.push(["让对方进入", payload.hotspotRunning
        ? "让对方先连接当前热点，再扫码或打开访问地址。"
        : "让对方保持在同一个网络里，再扫码或打开访问地址。"]);
    } else {
      nextSteps.push(["保持共享", "对方已经连接。你可以继续共享，或在完成后结束本次共享。"]);
    }

    if (attention) {
      nextSteps.push([attention.title, attention.text]);
    } else if (payload.hotspotRunning) {
      nextSteps.push(["热点信息", "当前热点已经启动。把热点名称和密码告诉对方后，再让对方扫码或打开地址。"]);
    } else {
      nextSteps.push(["访问方式", "建议优先让对方直接打开地址；如果现场更方便，也可以点“显示二维码”给对方扫码。"]);
    }
    renderSuggestions("shareNextSteps", nextSteps);

    const startBtn = $("shareStartBtn");
    if (startBtn) {
      startBtn.disabled = isHostSharing(payload);
    }
    const stopBtn = $("shareStopBtn");
    if (stopBtn) {
      stopBtn.disabled = !payload.serverRunning;
    }
  }

  function buildShareStatusSimple(payload) {
    const captureLabel = captureLabelValue(payload);
    const bridgeBusy = hasPendingHostBridgeCommand();

    if (isCaptureSelecting(payload)) {
      return {
        title: "正在等待你选择共享内容",
        detail: guidedState.hostWindowHint || "系统共享选择器已经打开，请选择要共享的窗口或屏幕。",
        badge: "选择中",
        tone: "warn",
      };
    }

    if (bridgeBusy) {
      return {
        title: "正在准备共享",
        detail: guidedState.hostWindowHint || "正在连接共享页面，请稍候。",
        badge: "处理中",
        tone: "warn",
      };
    }

    if (isHostSharing(payload) && Number(payload.viewers || 0) > 0) {
      return {
        title: "已有设备连接",
        detail: captureLabel
          ? "正在共享“" + captureLabel + "”，对方设备已经接入。你可以继续共享，或结束本次共享。"
          : "共享已经开始，对方设备已经接入。你可以继续共享，或结束本次共享。",
        badge: "已连接",
        tone: "ok",
      };
    }

    if (isHostSharing(payload)) {
      return {
        title: "正在共享",
        detail: captureLabel
          ? "正在共享“" + captureLabel + "”，等待对方连接。你可以让对方扫码或打开地址进入。"
          : "共享已经开始，正在等待对方连接。你可以让对方扫码或打开地址进入。",
        badge: "共享中",
        tone: "ok",
      };
    }

    if (guidedState.justStopped && !payload.serverRunning) {
      return {
        title: "共享已停止",
        detail: "本次共享已经结束。你可以重新开始，或返回引导重新选择连接方式。",
        badge: "已停止",
        tone: "warn",
      };
    }

    if (baseServiceReady(payload)) {
      return {
        title: "等待选择共享内容",
        detail: captureLabel
          ? "当前已选择“" + captureLabel + "”。你可以直接开始共享，或重新选择共享窗口。"
          : (guidedState.hostWindowHint || "点击“开始共享”或“选择共享窗口”，然后在系统弹窗中选择要共享的内容。"),
        badge: "待开始",
        tone: "idle",
      };
    }

    if (payload.serverRunning) {
      return {
        title: "共享前还差一步",
        detail: payload.dashboardError || "共享服务已经启动，但还有一项准备尚未完成。",
        badge: "需修复",
        tone: "warn",
      };
    }

    return {
      title: "请先准备共享环境",
      detail: "返回引导页面，系统会先帮你准备网络路径和共享服务，再进入这里选择共享内容。",
      badge: "未准备",
      tone: "idle",
    };
  }

  function renderShareSimple(payload) {
    const status = buildShareStatusSimple(payload);
    const attention = buildShareAttention(payload);
    const captureLabel = captureLabelValue(payload);
    const selecting = isCaptureSelecting(payload);
    const bridgeBusy = hasPendingHostBridgeCommand();

    setText("shareTitle", status.title);
    setText("shareDetail", status.detail);
    setStatusBadge("shareBadge", status.badge, status.tone);
    setText(
      "shareHelperText",
      selecting
        ? "共享选择器已经打开，请在系统弹窗中选择窗口或屏幕。完成后会自动开始共享。"
        : bridgeBusy
        ? (guidedState.hostWindowHint || "正在连接共享页面，请稍候。")
        : isHostSharing(payload)
        ? (captureLabel
          ? "当前正在共享“" + captureLabel + "”。你可以继续共享，也可以更换共享窗口。"
          : "当前已经开始共享。你可以继续共享，也可以更换共享窗口。")
        : (guidedState.hostWindowHint ||
          (payload.hotspotRunning
            ? "请让接收方先连接热点，再扫码或打开地址。共享内容需要在系统弹窗中选择。"
            : "你可以先把地址交给对方，再开始共享窗口或屏幕。"))
    );

    const shareAttentionCard = $("shareAttentionCard");
    if (shareAttentionCard) {
      shareAttentionCard.hidden = !attention;
    }
    setText("shareAttentionTitle", attention ? attention.title : "共享前还差一步");
    setText("shareAttentionText", attention ? attention.text : "当前没有额外提醒。");

    const shareAttentionBtn = $("shareAttentionBtn");
    if (shareAttentionBtn) {
      shareAttentionBtn.dataset.guidedAction = attention ? attention.action : "request-snapshot";
      shareAttentionBtn.textContent = attention ? attention.actionLabel : "刷新状态";
    }

    setPairs("shareAccessCard", [
      ["访问地址", payload.viewerUrl || "(准备后自动生成)"],
      ["二维码", payload.viewerUrl ? "点击“显示二维码”打开" : "准备后自动生成"],
      ["热点名称", payload.hotspotRunning ? (payload.hotspotSsid || "(未命名)") : "未使用"],
      ["热点密码", payload.hotspotRunning ? (payload.hotspotPassword || "(未生成)") : "未使用"]
    ]);

    setPairs("shareMethodCard", [
      ["当前路径", detectConnectionPath(payload)],
      ["共享地址", payload.hostIp || "(等待检测)"],
      ["热点状态", payload.hotspotStatus || "stopped"],
      ["本地健康", yesNo(payload.healthReady, "正常", "需注意")]
    ]);

    setPairs("shareSessionCard", [
      ["当前状态", selecting ? "等待选择共享内容" : (bridgeBusy ? "正在准备共享" : (isHostSharing(payload) ? "正在共享" : (payload.serverRunning ? "等待选择内容" : "未启动")))],
      ["共享目标", captureLabel || (selecting ? "等待选择中" : "尚未选择")],
      ["连接人数", payload.viewers],
      ["交付状态", payload.handoffLabel || "未开始"],
      ["最近提示", payload.handoffDetail || "准备完成后会提示下一步"]
    ]);

    const nextSteps = [];
    if (selecting) {
      nextSteps.push(["选择共享内容", "请在系统弹出的共享选择器中选择窗口或屏幕。选择完成后会自动开始共享。"]);
    } else if (bridgeBusy) {
      nextSteps.push(["正在准备共享", "系统正在连接共享页面。如果几秒内仍无弹窗，会自动切换到独立共享窗口。"]);
    } else if (!isHostSharing(payload)) {
      nextSteps.push(["开始共享内容", "点击上方的“开始共享”或“选择共享窗口”，然后在系统弹窗里选择要共享的窗口或屏幕。"]);
    } else if (Number(payload.viewers || 0) === 0) {
      nextSteps.push(["让对方进入", payload.hotspotRunning
        ? "让对方先连接当前热点，再扫码或打开访问地址。"
        : "让对方保持在同一个网络里，再扫码或打开访问地址。"]);
    } else {
      nextSteps.push(["保持共享", "对方已经连接。你可以继续共享，或在完成后结束本次共享。"]);
    }

    if (attention) {
      nextSteps.push([attention.title, attention.text]);
    } else if (payload.hotspotRunning) {
      nextSteps.push(["热点信息", "当前热点已经启动。把热点名称和密码告诉对方后，再让对方扫码或打开地址。"]);
    } else {
      nextSteps.push(["访问方式", "建议优先让对方直接打开地址；如果现场更方便，也可以点“显示二维码”给对方扫码。"]);
    }
    renderSuggestions("shareNextSteps", nextSteps);

    const startBtn = $("shareStartBtn");
    if (startBtn) {
      startBtn.disabled = bridgeBusy || isHostSharing(payload) || selecting;
      startBtn.textContent = selecting ? "正在选择…" : (bridgeBusy ? "正在准备…" : (isHostSharing(payload) ? "已经开始共享" : "开始共享"));
    }
    const stopBtn = $("shareStopBtn");
    if (stopBtn) {
      stopBtn.disabled = (bridgeBusy && !selecting) || (!payload.serverRunning && !isHostSharing(payload) && !selecting);
    }
    const chooseBtn = $("shareChooseBtn");
    if (chooseBtn) {
      chooseBtn.disabled = bridgeBusy || selecting;
      chooseBtn.textContent = bridgeBusy ? "正在准备…" : (isHostSharing(payload) ? "更换共享窗口" : "选择共享窗口");
    }
  }

  function renderShareDebug(payload) {
    setPairs("shareDebugMeta", [
      ["route", currentRoute],
      ["previewLoaded", previewLoaded],
      ["hostBridge.ready", hostBridge.ready],
      ["pendingCommand", hostBridge.pendingCommand ? (hostBridge.pendingCommand.command + " / " + hostBridge.pendingCommand.requestId) : "-"],
      ["inFlightRequestId", hostBridge.inFlightRequestId || "-"],
      ["hostUrl", payload.hostUrl || "-"],
      ["hostState", payload.hostState || "-"],
      ["captureState", payload.captureState || "-"],
      ["captureLabel", payload.captureLabel || "-"]
    ]);
    setText("shareDebugLog", debugState.entries.length ? debugState.entries.join("\n") : "No debug entries yet.");
  }

  function renderSimpleMode(payload) {
    renderGuide(payload);
    renderPrepare(payload);
    renderShareSimple(payload);
    renderShareDebug(payload);
  }

  function applySession() {
    sessionDirty = false;
    send({
      kind: "command",
      command: "apply-session",
      room: $("roomInput") ? $("roomInput").value || "" : "",
      token: $("tokenInput") ? $("tokenInput").value || "" : "",
      bind: $("bindInput") ? $("bindInput").value || "" : "",
      port: Number(($("portInput") ? $("portInput").value : "") || 9443)
    });
  }

  function applyHotspot() {
    hotspotDirty = false;
    send({
      kind: "command",
      command: "apply-hotspot",
      ssid: $("hotspotSsidInput") ? $("hotspotSsidInput").value || "" : "",
      password: $("hotspotPasswordInput") ? $("hotspotPasswordInput").value || "" : ""
    });
  }

  function syncHotspotActions(payload) {
    const startBtn = $("startHotspotBtn");
    const stopBtn = $("stopHotspotBtn");
    const applyBtn = $("applyHotspotBtn");
    if (!startBtn || !stopBtn || !applyBtn) return;

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
    debugLog("handleCommand", { command, extra: extra || null });
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

  function openGuidedShareWindow() {
    guidedState.justStopped = false;
    guidedState.pendingHostOpen = true;
    if (isHostSharing(state)) {
      guidedState.hostWindowHint = "共享窗口已打开。如果要更换共享内容，请先在共享窗口里点击 Stop Sharing，再点击 Start Sharing 重新选择窗口或屏幕。";
      handleCommand("open-host");
    } else if (!state.serverRunning) {
      guidedState.hostWindowHint = "系统会先打开共享窗口。接下来请在新打开的共享窗口里点击 Start Sharing，再选择窗口或屏幕。";
      handleCommand("start-and-open-host");
    } else {
      guidedState.hostWindowHint = "共享窗口已打开。请在新打开的共享窗口里点击 Start Sharing，然后选择窗口或屏幕。";
      handleCommand("open-host");
    }
    switchRoute("share");
    requestRender();
  }

  function runGuidedAction(action) {
    debugLog("runGuidedAction", {
      action,
      route: currentRoute,
      disabledStart: $("shareStartBtn") ? $("shareStartBtn").disabled : null,
      disabledChoose: $("shareChooseBtn") ? $("shareChooseBtn").disabled : null,
      disabledStop: $("shareStopBtn") ? $("shareStopBtn").disabled : null,
    });
    switch (action) {
      case "resume-share":
        switchRoute("share");
        requestRender();
        return;
      case "go-guide":
        switchRoute("guide");
        requestRender();
        return;
      case "switch-hotspot":
        guidedState.activeMode = "hotspot";
        guidedState.notice = "正在改用本机热点准备连接信息。";
        guidedState.issue = null;
        guidedState.status = "working";
        guidedState.actions.hotspotAuto = false;
        guidedState.actions.hotspotStarted = false;
        guidedState.lastCommandAt = 0;
        requestRender();
        return;
      case "retry-hotspot":
        guidedState.issue = null;
        guidedState.status = "working";
        guidedState.actions.hotspotStarted = false;
        guidedState.lastCommandAt = 0;
        requestRender();
        return;
      case "retry-service":
        guidedState.issue = null;
        guidedState.status = "working";
        guidedState.actions.serverStarted = false;
        guidedState.lastCommandAt = 0;
        handleCommand("stop-server");
        requestRender();
        return;
      case "open-share":
      case "choose-share":
        startGuidedShareCommand(action === "choose-share" ? "choose-share" : "start-share");
        return;
      case "stop-share":
        stopGuidedShareCommand();
        return;
        guidedState.pendingHostOpen = false;
        guidedState.justStopped = true;
        guidedState.hostWindowHint = "共享已停止。你可以重新开始，或返回引导重新选择连接方式。";
        handleCommand("stop-server");
        requestRender();
        return;
      default:
        break;
    }

    handleCommand(action);
  }

  function handleLocalAction(action) {
    if (action === "clear-share-debug") {
      debugState.entries = [];
      debugLog("debug log cleared");
      requestRender();
      return;
    }
    if (action === "reload-preview") {
      const frame = $("hostPreviewFrame");
      if (!frame) return;
      if (!state.serverRunning || !state.hostUrl) {
        handleCommand("request-snapshot");
        return;
      }
      previewLoaded = false;
      hostBridge.ready = false;
      previewUrl = state.hostUrl;
      frame.src = state.hostUrl;
      updateHostPreview(state);
    }
  }

  function resolveButtonFromEventTarget(target) {
    if (target && typeof target.closest === "function") {
      return target.closest("button");
    }
    if (target && target.parentElement && typeof target.parentElement.closest === "function") {
      return target.parentElement.closest("button");
    }
    return null;
  }

  function bindButtons() {
    document.addEventListener("click", (event) => {
      const button = resolveButtonFromEventTarget(event.target);
      if (!button) return;

      if (button.hasAttribute("data-route-target")) {
        switchRoute(button.getAttribute("data-route-target"));
        requestRender();
        return;
      }

      if (button.hasAttribute("data-guided-choice")) {
        startGuidedFlow(button.getAttribute("data-guided-choice"));
        return;
      }

      if (button.hasAttribute("data-guided-action")) {
        runGuidedAction(button.getAttribute("data-guided-action"));
        return;
      }

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

    const shareStartBtn = $("shareStartBtn");
    if (shareStartBtn) {
      shareStartBtn.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        debugLog("shareStartBtn click", { disabled: shareStartBtn.disabled });
        if (!shareStartBtn.disabled) {
          runGuidedAction("open-share");
        }
      });
    }

    const shareStopBtn = $("shareStopBtn");
    if (shareStopBtn) {
      shareStopBtn.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        debugLog("shareStopBtn click", { disabled: shareStopBtn.disabled });
        if (!shareStopBtn.disabled) {
          runGuidedAction("stop-share");
        }
      });
    }

    const shareChooseBtn = $("shareChooseBtn");
    if (shareChooseBtn) {
      shareChooseBtn.addEventListener("click", (event) => {
        event.preventDefault();
        event.stopPropagation();
        debugLog("shareChooseBtn click", { disabled: shareChooseBtn.disabled });
        if (!shareChooseBtn.disabled) {
          runGuidedAction("choose-share");
        }
      });
    }

    ["roomInput", "tokenInput", "bindInput", "portInput"].forEach((id) => {
      const node = $(id);
      if (!node) return;
      node.addEventListener("input", () => {
        sessionDirty = true;
      });
    });

    ["hotspotSsidInput", "hotspotPasswordInput"].forEach((id) => {
      const node = $(id);
      if (!node) return;
      node.addEventListener("input", () => {
        hotspotDirty = true;
      });
    });

    const previewFrame = $("hostPreviewFrame");
    if (previewFrame) {
      previewFrame.addEventListener("load", () => {
        debugLog("hostPreviewFrame load", { src: previewFrame.src || "" });
        previewLoaded = true;
        updateHostPreview(state);
        tryFlushHostBridgeCommand();
      });
    }

    document.querySelectorAll("[data-language-select]").forEach((select) => {
      if (select.dataset.boundLanguage === "true") return;
      select.dataset.boundLanguage = "true";
      select.addEventListener("change", () => {
        const locale = select.value || "en";
        if (window.LanShareI18n && typeof window.LanShareI18n.setLocale === "function") {
          window.LanShareI18n.setLocale(locale);
        }
        if (window.chrome && window.chrome.webview) {
          send({ kind: "command", command: "set-language", locale });
        }
      });
    });
  }

  function bindBridge() {
    if (!window.chrome || !window.chrome.webview) {
      setText("bridgeStatus", "Running outside WebView2");
      return;
    }

    window.chrome.webview.addEventListener("message", (event) => {
      const message = event.data || {};
      if (message.name === "state.snapshot") {
        debugLog("recv state.snapshot", {
          serverRunning: !!(message.payload && message.payload.serverRunning),
          hostUrl: message.payload && message.payload.hostUrl ? message.payload.hostUrl : "",
          hostState: message.payload && message.payload.hostState ? message.payload.hostState : "",
          captureState: message.payload && message.payload.captureState ? message.payload.captureState : "",
        });
        render(message.payload || {});
      }
    });

    window.addEventListener("message", (event) => {
      const message = event.data || {};
      if (message.source !== "lan-share-host") {
        return;
      }
      debugLog("recv host message", {
        kind: message.kind || "",
        requestId: message.requestId || "",
        state: message.state || "",
        captureState: message.captureState || "",
        captureLabel: message.captureLabel || "",
      });

      if (message.kind === "bridge-ready") {
        hostBridge.ready = true;
        mergeHostBridgePatch({
          hostState: message.state || state.hostState,
          captureState: Object.prototype.hasOwnProperty.call(message, "captureState") ? message.captureState : state.captureState,
          captureLabel: Object.prototype.hasOwnProperty.call(message, "captureLabel") ? message.captureLabel : state.captureLabel,
        });
        tryFlushHostBridgeCommand();
        return;
      }

      if (message.kind === "status") {
        hostBridge.ready = true;
        mergeHostBridgePatch({
          hostState: message.state || state.hostState,
          viewers: Object.prototype.hasOwnProperty.call(message, "viewers") ? message.viewers : state.viewers,
          captureState: Object.prototype.hasOwnProperty.call(message, "captureState") ? message.captureState : state.captureState,
          captureLabel: Object.prototype.hasOwnProperty.call(message, "captureLabel") ? message.captureLabel : state.captureLabel,
        });
        tryFlushHostBridgeCommand();
        return;
      }

      if (message.kind === "command-result") {
        mergeHostBridgePatch({
          hostState: message.state || state.hostState,
          viewers: Object.prototype.hasOwnProperty.call(message, "viewers") ? message.viewers : state.viewers,
          captureState: Object.prototype.hasOwnProperty.call(message, "captureState") ? message.captureState : state.captureState,
          captureLabel: Object.prototype.hasOwnProperty.call(message, "captureLabel") ? message.captureLabel : state.captureLabel,
        });

        if (!hostBridgeRequestMatches(message.requestId)) {
          return;
        }

        const pending = hostBridge.pendingCommand;
        finalizeHostBridgeRequest(message.requestId);
        guidedState.pendingHostOpen = false;
        const detailText = String(message.detail || "").toLowerCase();
        const selectionCanceled =
          detailText.includes("cancel") ||
          detailText.includes("denied") ||
          detailText.includes("dismiss") ||
          detailText.includes("abort");

        if (message.ok) {
          if (message.command === "stop-share" && pending && pending.stopServerAfter) {
            guidedState.hostWindowHint = "正在结束本次共享。";
            handleCommand("stop-server");
          } else if (message.command === "choose-share") {
            guidedState.hostWindowHint = "共享目标已更新。";
          } else if (message.command === "start-share") {
            guidedState.hostWindowHint = "共享已经开始，等待对方连接。";
          }
        } else if (pending && pending.stopServerAfter) {
          guidedState.hostWindowHint = "共享已停止。";
          handleCommand("stop-server");
        } else if (selectionCanceled) {
          guidedState.hostWindowHint = "你已取消共享选择，可以重新开始。";
        } else if (pending) {
          fallbackOpenHostWindow(pending.command);
          finalizeHostBridgeRequest(message.requestId);
          return;
        } else {
          guidedState.hostWindowHint = "共享启动失败，请重试或进入高级诊断。";
        }

        requestRender();
      }
    });

    send({ kind: "ready" });
  }

  function renderAdvancedShell(payload) {
    const nativeTab = normalizeTab(state.nativePage);
    if (nativeTab !== activeTab) {
      switchTab(nativeTab, false);
    }
    setText("bridgeStatus", state.serverRunning ? "Bridge live / service running" : "Bridge live");
    const bridgeNode = $("bridgeStatus");
    if (bridgeNode) {
      bridgeNode.className = "status " + (state.serverRunning ? "ok" : "idle");
    }

    const detailLine = state.dashboardState === "ready"
      ? "Service and host page look ready for the next sharing session."
      : state.dashboardState === "sharing"
      ? "Sharing is active. Viewer link can be handed off right now."
      : state.dashboardState === "error"
      ? "The service is up, but live checks are still failing."
      : "The operator still needs to complete setup before sharing.";

    setText("dashboardStateLabel", state.dashboardLabel || "Unknown");
    const dashboardLabel = $("dashboardStateLabel");
    if (dashboardLabel) {
      dashboardLabel.className = "status-label tone-" + (state.dashboardState || "not-ready");
    }
    setText("dashboardStateDetail", detailLine);

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
      ["Capture State", state.captureState],
      ["Capture Target", state.captureLabel],
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
      ["Capture State", state.captureState],
      ["Capture Target", state.captureLabel],
      ["Local Reachability", state.localReachability]
    ]);
    updateHostPreview(state);

    setPairs("networkSummary", [
      ["Recommended IPv4", state.hostIp],
      ["Current Bind", state.bind],
      ["Reachability", state.localReachability],
      ["Firewall Path", state.firewallReady ? "Ready" : "Needs attention"],
      ["Remote Viewer Path", state.remoteViewerReady ? "Ready" : "Needs attention"],
      ["Wi-Fi Adapter", state.wifiAdapterPresent],
      ["Hotspot Supported", state.hotspotSupported],
      ["Current Hotspot State", state.hotspotStatus]
    ]);

    setPairs("networkDiagnosticsCard", [
      ["Firewall Path", state.firewallReady ? "Ready" : "Needs attention"],
      ["Firewall Detail", state.firewallDetail],
      ["Remote Viewer Path", state.remoteViewerReady ? "Ready" : "Needs attention"],
      ["Remote Viewer Detail", state.remoteViewerDetail],
      ["Probe Summary", state.remoteProbeLabel],
      ["Next Action", state.remoteProbeAction]
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

    setHtml("sharingGuide", sharingGuide(state).map(([label, detail]) => {
      return "<article class=\"guide\"><h3>" + label + "</h3><p>" + detail + "</p></article>";
    }).join(""));

    renderMetrics(state);
    setText("timelineText", state.timelineText || "No timeline events yet.");
    setText("logTailText", state.logTail || "No logs yet.");

    renderChecklist(state);
    const diagActions = [
      ["Check subnet alignment", "Confirm the viewer device and the host still sit on the same local network."]
    ];
    if (state.serverRunning && !state.firewallReady) {
      diagActions.push(["Open Windows Firewall settings", state.firewallDetail || "Confirm an inbound allow rule exists for the current share executable or TCP port."]);
    }
    if (state.serverRunning && !state.remoteViewerReady) {
      diagActions.push(["Collect a local network diagnostics report", state.remoteViewerDetail || "Run the helper report before retrying from another device."]);
      diagActions.push(["Export a remote-device probe guide", state.remoteProbeAction || "Generate a checklist with candidate LAN URLs and remote browser test steps."]);
    }
    diagActions.push(["Test URL directly", "If Viewer fails, paste the Viewer URL directly into a browser first."]);
    diagActions.push(["Fallback to system hotspot", "If hotspot start fails, open the Windows hotspot settings and start it manually."]);
    renderSuggestions("diagActions", diagActions);
    setPairs("diagPaths", [
      ["Output Dir", state.outputDir],
      ["Bundle Dir", state.bundleDir],
      ["Cert Dir", state.certDir],
      ["Server EXE", state.serverExePath]
    ]);
    setText("diagWarningText", state.dashboardError || "None");

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

    setText("rawState", JSON.stringify(payload || {}, null, 2));
  }

  function render(payload) {
    Object.assign(state, payload || {});
    if (state.locale && window.LanShareI18n && typeof window.LanShareI18n.setLocale === "function") {
      window.LanShareI18n.setLocale(state.locale, { persist: false });
    }
    updateRouteFromSnapshot(state);
    syncGuidedShareFlags(state);
    renderSimpleMode(state);
    renderAdvancedShell(state);
    runGuidedPreparation(state);
    tryFlushHostBridgeCommand();
    if (window.LanShareI18n && typeof window.LanShareI18n.applyDocument === "function") {
      window.LanShareI18n.applyDocument();
    }
  }

  bindButtons();
  switchRoute(currentRoute);
  switchTab(activeTab, false);
  debugLog("simple shell initialized", { route: currentRoute });
  bindBridge();
})();
