/**
 * Host page:
 * - registers the host over WSS
 * - captures the screen with getDisplayMedia
 * - forwards WebRTC offers/ICE to viewers
 * - behaves like an installable fullscreen sender console when the browser allows it
 */

const room = qs("room");
const token = qs("token");

const hostShell = document.getElementById("hostShell");
const infoEl = document.getElementById("info");
const btnStart = document.getElementById("btnStart");
const btnStop = document.getElementById("btnStop");
const btnInstall = document.getElementById("btnInstall");
const btnFullscreen = document.getElementById("btnFullscreen");
const btnToggleChrome = document.getElementById("btnToggleChrome");
const btnToggleDebug = document.getElementById("btnToggleDebug");
const btnCloseDebug = document.getElementById("btnCloseDebug");
const debugPanel = document.getElementById("debugPanel");
const viewersEl = document.getElementById("viewers");
const statePill = document.getElementById("statePill");
const displayModePill = document.getElementById("displayModePill");
const roomEl = document.getElementById("room");
const factRoom = document.getElementById("factRoom");
const factToken = document.getElementById("factToken");
const factSignal = document.getElementById("factSignal");
const factCapture = document.getElementById("factCapture");
const hostHintText = document.getElementById("hostHintText");

roomEl.textContent = room || "(missing)";
factRoom.textContent = room || "(missing)";
factToken.textContent = token || "(missing)";
document.title = room ? `LAN Host - ${room}` : "LAN Host";

let ws = null;
const peers = new Map();

let screenStream = null;
let registered = false;
let state = "idle";
let chromeHideTimer = null;
let deferredInstallPrompt = null;
let wakeLock = null;

function isStandaloneDisplay() {
  return (
    (window.matchMedia && window.matchMedia("(display-mode: standalone)").matches) ||
    (window.matchMedia && window.matchMedia("(display-mode: fullscreen)").matches) ||
    window.navigator.standalone === true
  );
}

function setHint(text) {
  if (hostHintText) {
    hostHintText.textContent = text;
  }
}

function updateDisplayMode() {
  const fullscreen = !!document.fullscreenElement;
  const standalone = isStandaloneDisplay();
  if (displayModePill) {
    displayModePill.textContent = fullscreen ? "fullscreen" : (standalone ? "installed" : "browser");
  }
  if (btnFullscreen) {
    btnFullscreen.textContent = fullscreen ? "Exit Fullscreen" : "Enter Fullscreen";
  }
}

function setChromeHidden(hidden) {
  if (!hostShell) return;
  hostShell.classList.toggle("host-chrome-hidden", hidden);
  if (btnToggleChrome) {
    btnToggleChrome.textContent = hidden ? "Show HUD" : "Hide HUD";
  }
}

function scheduleChromeHide() {
  if (chromeHideTimer) {
    clearTimeout(chromeHideTimer);
  }
  chromeHideTimer = setTimeout(() => {
    if (screenStream && (document.fullscreenElement || isStandaloneDisplay())) {
      setChromeHidden(true);
    }
  }, 2400);
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
    } else if (hostShell && typeof hostShell.requestFullscreen === "function") {
      await hostShell.requestFullscreen();
      await tryLockLandscape("fullscreen");
      scheduleChromeHide();
    }
    updateDisplayMode();
  } catch (e) {
    log(`fullscreen failed: ${e}`);
  }
}

async function registerHostShell() {
  if (!("serviceWorker" in navigator) || !window.isSecureContext) return;
  try {
    await navigator.serviceWorker.register("/host-sw.js", { scope: "/host" });
    log("host shell registered");
  } catch (e) {
    log(`host shell registration failed: ${e}`);
  }
}

function syncNativeStatus() {
  pushHostStatus(state, {
    room,
    viewers: peers.size,
    sharing: !!screenStream,
  });
}

function setState(nextState) {
  state = nextState;
  if (statePill) {
    statePill.textContent = nextState;
    statePill.dataset.state = nextState;
  }
  if (factSignal) {
    factSignal.textContent = nextState;
  }
  if (hostShell) {
    hostShell.classList.toggle("host-sharing", nextState === "sharing");
  }
  syncNativeStatus();
}

function updateViewers() {
  const count = String(peers.size);
  if (viewersEl) viewersEl.textContent = count;
  syncNativeStatus();
}

function updateCaptureState(label) {
  if (factCapture) {
    factCapture.textContent = label;
  }
}

function wsSend(obj) {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify(obj));
}

function setConfigError(message) {
  if (infoEl) {
    infoEl.textContent = message;
  }
  log(message);
  setState("config_error");
  btnStart.disabled = true;
  btnStop.disabled = true;
  setHint("This host link is incomplete. Re-open the sharing session from the desktop app or bundle.");
}

function showInstallHint() {
  if (deferredInstallPrompt && btnInstall) {
    btnInstall.hidden = false;
    setHint("Install the host or open fullscreen to hide most browser chrome and make the sender feel like a native control surface.");
  } else if (isStandaloneDisplay()) {
    setHint("Installed mode is active. Tap the background to reveal or hide the control HUD.");
  } else if (screenStream) {
    setHint("Sharing is active. Enter fullscreen or install the host app for a cleaner sender experience.");
  } else {
    setHint("Start sharing after the host finishes registering. Install or fullscreen mode will remove most browser chrome on supported devices.");
  }
}

function startSignal() {
  const missing = [];
  if (!room) missing.push("room");
  if (!token) missing.push("token");

  if (missing.length) {
    setConfigError(
      `Missing URL parameter(s): ${missing.join(", ")}. Open /host?room=<ROOM>&token=<TOKEN>.`
    );
    return;
  }

  ws = new WebSocket(wssUrl("/ws"));

  ws.onopen = () => {
    log("WSS connected");
    setState("registering");
    wsSend({ type: "host.register", room, token });
  };

  ws.onmessage = async (ev) => {
    const msg = JSON.parse(ev.data);

    if (msg.type === "host.registered") {
      registered = true;
      setState("ready");
      if (infoEl) {
        infoEl.textContent = "Host registered. Start sharing when you are ready to broadcast this session.";
      }
      showInstallHint();
      return;
    }

    if (msg.type === "peer.joined") {
      const peerId = msg.peerId;
      log(`peer joined: ${peerId}`);
      await ensurePeer(peerId);
      updateViewers();
      scheduleChromeHide();
      return;
    }

    if (msg.type === "peer.left") {
      const peerId = msg.peerId;
      log(`peer left: ${peerId}`);
      cleanupPeer(peerId);
      updateViewers();
      return;
    }

    if (msg.type === "webrtc.answer") {
      const peerId = msg.from;
      const peer = peers.get(peerId);
      if (!peer) return;

      try {
        await peer.pc.setRemoteDescription({ type: "answer", sdp: msg.sdp });
        log(`setRemoteDescription(answer) ok: ${peerId}`);
      } catch (e) {
        log(`setRemoteDescription(answer) failed: ${peerId} ${e}`);
      }

      if (peer.needsRenegotiate && peer.pc.signalingState === "stable") {
        peer.needsRenegotiate = false;
        negotiatePeer(peerId);
      }
      return;
    }

    if (msg.type === "webrtc.ice") {
      const peerId = msg.from;
      const peer = peers.get(peerId);
      if (!peer) return;

      try {
        await peer.pc.addIceCandidate(msg.candidate);
      } catch (e) {
        log(`addIceCandidate failed: ${peerId} ${e}`);
      }
      return;
    }

    if (msg.type === "session.end.ack") {
      log(`session.end acknowledged for room=${msg.room || room}`);
      return;
    }

    if (msg.type === "error") {
      log(`ERROR ${msg.code}: ${msg.message}`);
      setState("error");
    }
  };

  ws.onclose = () => {
    log("WSS closed");
    setState("closed");
    setChromeHidden(false);
  };

  ws.onerror = () => {
    log("WSS error");
    setState("error");
  };
}

function attachStreamToPeer(peer, stream) {
  const videoTrack = stream?.getVideoTracks?.()[0];
  if (!videoTrack) return false;

  if (peer.videoSender) {
    peer.videoSender.replaceTrack(videoTrack);
  } else {
    peer.videoSender = peer.pc.addTrack(videoTrack, stream);
  }
  return true;
}

async function ensurePeer(peerId) {
  if (peers.has(peerId)) return;

  const pc = new RTCPeerConnection({ iceServers: [] });
  const peer = {
    pc,
    negotiating: false,
    needsRenegotiate: false,
    videoSender: null,
  };
  peers.set(peerId, peer);

  pc.onicecandidate = (ev) => {
    if (!ev.candidate) return;
    wsSend({ type: "webrtc.ice", room, token, to: peerId, candidate: ev.candidate });
  };

  pc.onconnectionstatechange = () => {
    log(`pc(${peerId}) connectionState=${pc.connectionState} ice=${pc.iceConnectionState}`);
  };

  pc.onsignalingstatechange = () => {
    if (peer.needsRenegotiate && pc.signalingState === "stable") {
      peer.needsRenegotiate = false;
      negotiatePeer(peerId);
    }
  };

  pc.onnegotiationneeded = () => {
    peer.needsRenegotiate = true;
    negotiatePeer(peerId);
  };

  if (screenStream) {
    attachStreamToPeer(peer, screenStream);
  }
}

function cleanupPeer(peerId) {
  const peer = peers.get(peerId);
  if (peer?.pc) {
    try {
      peer.pc.close();
    } catch {}
  }
  peers.delete(peerId);
}

async function negotiatePeer(peerId) {
  const peer = peers.get(peerId);
  if (!peer) return;

  if (peer.negotiating) {
    peer.needsRenegotiate = true;
    return;
  }

  if (peer.pc.signalingState !== "stable") {
    peer.needsRenegotiate = true;
    return;
  }

  peer.needsRenegotiate = false;
  peer.negotiating = true;
  try {
    const offer = await peer.pc.createOffer({ offerToReceiveAudio: false, offerToReceiveVideo: false });
    await peer.pc.setLocalDescription(offer);
    wsSend({ type: "webrtc.offer", room, token, to: peerId, sdp: offer.sdp });
    log(`sent offer to ${peerId}`);
  } catch (e) {
    log(`negotiate(${peerId}) failed: ${e}`);
  } finally {
    peer.negotiating = false;
    if (peer.needsRenegotiate) {
      peer.needsRenegotiate = false;
      setTimeout(() => negotiatePeer(peerId), 0);
    }
  }
}

async function startShare() {
  if (screenStream) return;

  screenStream = await navigator.mediaDevices.getDisplayMedia({ video: true, audio: false });
  log("getDisplayMedia OK");
  updateCaptureState("screen selected");

  const videoTrack = screenStream.getVideoTracks()[0];
  if (videoTrack) {
    updateCaptureState(videoTrack.label || "screen selected");
    videoTrack.onended = () => {
      log("screen track ended");
      stopShare("screen_ended");
    };
  }

  for (const [peerId, peer] of peers) {
    attachStreamToPeer(peer, screenStream);
  }

  setState("sharing");
  showInstallHint();
  scheduleChromeHide();
  await ensureWakeLock("sharing");
}

function stopShare(reason = "host_stopped") {
  if (screenStream) {
    try {
      for (const track of screenStream.getTracks()) track.stop();
    } catch {}
    screenStream = null;
  }

  wsSend({ type: "session.end", room, token, reason });

  for (const [peerId, peer] of peers) {
    try {
      peer.pc.close();
    } catch {}
    peers.delete(peerId);
  }

  updateViewers();
  updateCaptureState("not started");
  log(`session ended: ${reason}`);
  setState("ready");
  setChromeHidden(false);
}

btnStart.onclick = async () => {
  try {
    setState("starting_share");
    await startShare();
    btnStart.disabled = true;
    btnStop.disabled = false;
  } catch (e) {
    setState("ready");
    log(`startShare failed: ${e}`);
    showInstallHint();
  }
};

btnStop.onclick = () => {
  stopShare("host_stopped");
  btnStart.disabled = false;
  btnStop.disabled = true;
};

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

if (btnFullscreen) {
  btnFullscreen.onclick = async () => {
    await tryEnterFullscreen();
  };
}

if (btnToggleChrome) {
  btnToggleChrome.onclick = () => {
    const hidden = !hostShell.classList.contains("host-chrome-hidden");
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

window.addEventListener("beforeinstallprompt", (ev) => {
  ev.preventDefault();
  deferredInstallPrompt = ev;
  if (btnInstall) btnInstall.hidden = false;
  showInstallHint();
  log("host install prompt is ready");
});

window.addEventListener("appinstalled", () => {
  deferredInstallPrompt = null;
  if (btnInstall) btnInstall.hidden = true;
  updateDisplayMode();
  showInstallHint();
  log("host app installed");
});

document.addEventListener("fullscreenchange", () => {
  updateDisplayMode();
  if (!document.fullscreenElement) {
    setChromeHidden(false);
  } else {
    scheduleChromeHide();
  }
});

document.addEventListener("visibilitychange", () => {
  if (!document.hidden && screenStream) {
    void ensureWakeLock("visibility");
  }
});

document.addEventListener(
  "click",
  (ev) => {
    if (ev.target && typeof ev.target.closest === "function" && ev.target.closest("button")) {
      return;
    }
    if (!screenStream) return;
    const hidden = hostShell.classList.contains("host-chrome-hidden");
    setChromeHidden(!hidden);
    if (hidden) {
      scheduleChromeHide();
    }
  },
  { passive: true }
);

window.addEventListener("beforeunload", () => {
  try {
    if (registered) wsSend({ type: "session.end", room, token, reason: "host_unloaded" });
  } catch {}
});

if (window.matchMedia) {
  const media = window.matchMedia("(display-mode: standalone)");
  if (typeof media.addEventListener === "function") {
    media.addEventListener("change", updateDisplayMode);
  } else if (typeof media.addListener === "function") {
    media.addListener(updateDisplayMode);
  }
}

if (infoEl) {
  infoEl.innerHTML =
    'Open this page on HTTPS with <span class="mono">/host?room=&lt;ROOM&gt;&amp;token=&lt;TOKEN&gt;</span>, then start screen sharing.';
}

updateDisplayMode();
showInstallHint();
updateViewers();
updateCaptureState("not started");
setState("idle");
void registerHostShell();
startSignal();
