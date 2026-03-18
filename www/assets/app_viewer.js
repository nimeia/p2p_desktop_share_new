/**
 * Viewer page:
 * - joins a room over WS
 * - receives WebRTC video from the host
 * - behaves like an installable fullscreen receiver when the browser allows it
 */

const room = qs("room");
const translateUi = (text) => window.LanShareI18n && typeof window.LanShareI18n.translateText === "function"
  ? window.LanShareI18n.translateText(text)
  : text;
document.getElementById("room").textContent = room || "(missing)";
document.title = translateUi(room ? `LAN Viewer - ${room}` : "LAN Viewer");

const viewerShell = document.getElementById("viewerShell");
const video = document.getElementById("video");
const statePill = document.getElementById("statePill");
const displayModePill = document.getElementById("displayModePill");
const viewerHintText = document.getElementById("viewerHintText");
const btnInstall = document.getElementById("btnInstall");
const btnFullscreen = document.getElementById("btnFullscreen");
const btnPlay = document.getElementById("btnPlay");
const btnToggleChrome = document.getElementById("btnToggleChrome");
const btnToggleDebug = document.getElementById("btnToggleDebug");
const btnCloseDebug = document.getElementById("btnCloseDebug");
const debugPanel = document.getElementById("debugPanel");

video.playsInline = true;
video.autoplay = true;
video.muted = true;
video.defaultMuted = true;

let ws = null;
let peerId = null;
let pc = null;
let playRetryTimer = null;
let chromeHideTimer = null;
let deferredInstallPrompt = null;
let wakeLock = null;

function setState(nextState) {
  if (statePill) {
    statePill.textContent = nextState;
    statePill.dataset.state = nextState;
  }
}

function isStandaloneDisplay() {
  return (
    (window.matchMedia && window.matchMedia("(display-mode: standalone)").matches) ||
    (window.matchMedia && window.matchMedia("(display-mode: fullscreen)").matches) ||
    window.navigator.standalone === true
  );
}

function updateDisplayMode() {
  const fullscreen = !!document.fullscreenElement;
  const standalone = isStandaloneDisplay();
  if (viewerShell) {
    viewerShell.classList.toggle("standalone-mode", standalone || fullscreen);
  }
  if (displayModePill) {
    displayModePill.textContent = fullscreen ? "fullscreen" : (standalone ? "installed" : "browser");
  }
  if (btnFullscreen) {
    btnFullscreen.textContent = fullscreen ? "Exit Fullscreen" : "Enter Fullscreen";
  }
}

function setHint(text) {
  if (viewerHintText) {
    viewerHintText.textContent = text;
  }
}

function showPlayHint() {
  if (btnPlay) btnPlay.hidden = false;
  setHint("The browser blocked autoplay. Tap Play once, then the receiver will continue like a native viewer.");
}

function hidePlayHint() {
  if (btnPlay) btnPlay.hidden = true;
}

function showInstallHint() {
  if (deferredInstallPrompt && btnInstall) {
    btnInstall.hidden = false;
    setHint("Enter fullscreen for a cleaner receiver view and less browser chrome.");
  } else if (isStandaloneDisplay()) {
    setHint("Installed mode is active. Tap the picture to show or hide the HUD.");
  } else {
    setHint("Open fullscreen for a cleaner receiver view.");
  }
}

function setChromeHidden(hidden) {
  if (!viewerShell) return;
  viewerShell.classList.toggle("chrome-hidden", hidden);
  if (btnToggleChrome) {
    btnToggleChrome.textContent = hidden ? "Show HUD" : "Hide HUD";
  }
}

function scheduleChromeHide() {
  if (chromeHideTimer) {
    clearTimeout(chromeHideTimer);
  }
  chromeHideTimer = setTimeout(() => {
    if (video.srcObject && (document.fullscreenElement || isStandaloneDisplay())) {
      setChromeHidden(true);
    }
  }, 2200);
}

async function tryLockLandscape(reason) {
  if (!screen.orientation || typeof screen.orientation.lock !== "function") return;
  try {
    await screen.orientation.lock("landscape");
    log(`orientation locked (${reason})`);
  } catch (e) {
    log(`orientation lock skipped (${reason}): ${e}`);
  }
}

async function ensureWakeLock(reason) {
  if (!("wakeLock" in navigator)) return;
  try {
    if (wakeLock) return;
    wakeLock = await navigator.wakeLock.request("screen");
    wakeLock.addEventListener("release", () => {
      log(`wake lock released (${reason})`);
      wakeLock = null;
    });
    log(`wake lock acquired (${reason})`);
  } catch (e) {
    log(`wake lock skipped (${reason}): ${e}`);
  }
}

async function tryEnterFullscreen() {
  try {
    if (document.fullscreenElement) {
      await document.exitFullscreen();
    } else if (viewerShell && typeof viewerShell.requestFullscreen === "function") {
      await viewerShell.requestFullscreen();
      await tryLockLandscape("fullscreen");
      scheduleChromeHide();
    }
    updateDisplayMode();
  } catch (e) {
    log(`fullscreen failed: ${e}`);
  }
}

async function registerViewerShell() {
  if (!("serviceWorker" in navigator) || !window.isSecureContext) return;
  try {
    await navigator.serviceWorker.register("/viewer-sw.js", { scope: "/view" });
    log("viewer shell registered");
  } catch (e) {
    log(`viewer shell registration failed: ${e}`);
  }
}

function wsSend(obj) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify(obj));
}

async function tryPlayVideo(reason) {
  try {
    hidePlayHint();
    await video.play();
    log(`video.play OK (${reason})`);
    setState("playing");
    showInstallHint();
    scheduleChromeHide();
    await ensureWakeLock("playback");
  } catch (e) {
    const message = String(e || "");
    if ((e && e.name === "AbortError") || message.includes("interrupted by a call to pause")) {
      log(`video.play interrupted (${reason}): ${e}`);
      setState("stream_attached");
      if (playRetryTimer) {
        clearTimeout(playRetryTimer);
      }
      playRetryTimer = setTimeout(() => {
        playRetryTimer = null;
        if (video.srcObject) {
          void tryPlayVideo(`${reason}-retry`);
        }
      }, 150);
      return;
    }
    log(`video.play blocked (${reason}): ${e}`);
    setState("play_blocked");
    showPlayHint();
    setChromeHidden(false);
  }
}

function ensurePc() {
  if (pc && pc.connectionState !== "closed") return pc;

  pc = new RTCPeerConnection({ iceServers: [] });
  pc.addTransceiver("video", { direction: "recvonly" });

  pc.ontrack = (ev) => {
    log("ontrack");
    const stream = ev.streams && ev.streams[0] ? ev.streams[0] : new MediaStream([ev.track]);
    video.srcObject = stream;
    viewerShell.classList.add("is-playing");
    setState("stream_attached");
    void tryPlayVideo("ontrack");
  };

  pc.onicecandidate = (ev) => {
    if (!ev.candidate) return;
    wsSend({ type: "webrtc.ice", room, to: "host", candidate: ev.candidate });
  };

  pc.onconnectionstatechange = () => {
    log(`pc connectionState=${pc.connectionState} ice=${pc.iceConnectionState}`);
    if (pc.connectionState === "connected") {
      if (video.srcObject) {
        void tryPlayVideo("pc-connected");
      } else {
        setState("connected");
      }
      return;
    }
    if (pc.connectionState === "failed" || pc.connectionState === "disconnected") {
      setState("disconnected");
      setChromeHidden(false);
    }
  };

  return pc;
}

function cleanupPc() {
  if (playRetryTimer) {
    clearTimeout(playRetryTimer);
    playRetryTimer = null;
  }
  if (chromeHideTimer) {
    clearTimeout(chromeHideTimer);
    chromeHideTimer = null;
  }
  try {
    if (pc) pc.close();
  } catch {}
  pc = null;
  viewerShell.classList.remove("is-playing");
  try {
    video.srcObject = null;
  } catch {}
  hidePlayHint();
  setChromeHidden(false);
}

function startSignal() {
  if (!room) {
    log("Missing URL parameter: room. Open /view?room=<ROOM>.");
    setState("config_error");
    setHint("This viewer link is missing the room parameter. Re-open the Viewer URL from the sharing bundle.");
    return;
  }

  ws = new WebSocket(wssUrl("/ws"));

  ws.onopen = () => {
    log("WS connected");
    setState("connecting");
    wsSend({ type: "room.join", room });
  };

  ws.onmessage = async (ev) => {
    const msg = JSON.parse(ev.data);

    if (msg.type === "room.joined") {
      peerId = msg.peerId;
      log(`joined as ${peerId}`);
      setState("joined");
      showInstallHint();
      return;
    }

    if (msg.type === "webrtc.offer") {
      await onOffer(msg.sdp);
      return;
    }

    if (msg.type === "webrtc.ice") {
      if (!pc) return;
      try {
        await pc.addIceCandidate(msg.candidate);
      } catch (e) {
        log(`addIceCandidate failed: ${e}`);
      }
      return;
    }

    if (msg.type === "session.ended") {
      log(`session ended: ${msg.reason || ""}`);
      cleanupPc();
      setState("ended");
      setHint("The host ended this session. Keep the viewer open and reconnect after the host restarts sharing.");
      return;
    }

    if (msg.type === "error") {
      log(`ERROR ${msg.code}: ${msg.message}`);
      setState("error");
      setHint(msg.message || "The viewer hit an error. Re-open the shared Viewer URL after the host starts again.");
    }
  };

  ws.onclose = () => {
    log("WS closed");
    setState("closed");
    setChromeHidden(false);
  };

  ws.onerror = () => log("WS error");
}

async function onOffer(sdp) {
  setState("negotiating");

  const currentPc = ensurePc();

  try {
    await currentPc.setRemoteDescription({ type: "offer", sdp });
    const answer = await currentPc.createAnswer();
    await currentPc.setLocalDescription(answer);

    wsSend({ type: "webrtc.answer", room, to: "host", sdp: answer.sdp });
    log("sent answer");

    if (video.srcObject && currentPc.connectionState === "connected") {
      void tryPlayVideo("post-answer");
    }
  } catch (e) {
    log(`onOffer failed: ${e}`);
    setState("error");
    setChromeHidden(false);
  }
}

video.onloadedmetadata = () => {
  log(`video metadata ${video.videoWidth}x${video.videoHeight}`);
};

video.onplaying = async () => {
  log("video playing");
  hidePlayHint();
  setState("playing");
  showInstallHint();
  scheduleChromeHide();
  await ensureWakeLock("onplaying");
};

video.onerror = () => {
  const err = video.error;
  log(`video error code=${err ? err.code : "unknown"}`);
};

if (btnPlay) {
  btnPlay.onclick = async () => {
    await tryPlayVideo("manual-button");
  };
}

if (btnFullscreen) {
  btnFullscreen.onclick = async () => {
    await tryEnterFullscreen();
  };
}

if (btnInstall) {
  btnInstall.onclick = async () => {
    if (!deferredInstallPrompt) return;
    deferredInstallPrompt.prompt();
    const choice = await deferredInstallPrompt.userChoice;
    log(`install prompt result: ${choice && choice.outcome ? choice.outcome : "unknown"}`);
    deferredInstallPrompt = null;
    btnInstall.hidden = true;
    showInstallHint();
  };
}

if (btnToggleChrome) {
  btnToggleChrome.onclick = () => {
    const hidden = !viewerShell.classList.contains("chrome-hidden");
    setChromeHidden(hidden);
    if (!hidden) {
      scheduleChromeHide();
    }
  };
}

if (btnToggleDebug) {
  btnToggleDebug.onclick = () => {
    debugPanel.classList.toggle("is-hidden");
    setChromeHidden(false);
  };
}

if (btnCloseDebug) {
  btnCloseDebug.onclick = () => {
    debugPanel.classList.add("is-hidden");
  };
}

document.addEventListener(
  "click",
  (ev) => {
    if (ev.target && typeof ev.target.closest === "function" && ev.target.closest("button")) {
      return;
    }
    if (!video.srcObject) return;
    if (document.getElementById("statePill").textContent === "play_blocked") {
      void tryPlayVideo("document-click");
      return;
    }
    const hidden = viewerShell.classList.contains("chrome-hidden");
    setChromeHidden(!hidden);
    if (hidden) {
      scheduleChromeHide();
    }
  },
  { passive: true }
);

document.addEventListener("visibilitychange", () => {
  if (!document.hidden && video.srcObject) {
    void ensureWakeLock("visibility");
    showInstallHint();
  }
});

document.addEventListener("fullscreenchange", () => {
  updateDisplayMode();
  if (!document.fullscreenElement) {
    setChromeHidden(false);
  } else {
    scheduleChromeHide();
  }
});

window.addEventListener("beforeinstallprompt", (ev) => {
  ev.preventDefault();
  deferredInstallPrompt = ev;
  if (btnInstall) btnInstall.hidden = false;
  showInstallHint();
  log("viewer install prompt is ready");
});

window.addEventListener("appinstalled", () => {
  deferredInstallPrompt = null;
  if (btnInstall) btnInstall.hidden = true;
  updateDisplayMode();
  showInstallHint();
  log("viewer app installed");
});

if (window.matchMedia) {
  const media = window.matchMedia("(display-mode: standalone)");
  if (typeof media.addEventListener === "function") {
    media.addEventListener("change", updateDisplayMode);
  } else if (typeof media.addListener === "function") {
    media.addListener(updateDisplayMode);
  }
}

updateDisplayMode();
showInstallHint();
void registerViewerShell();
startSignal();
