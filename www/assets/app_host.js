/**
 * Host page:
 * - registers the host over WSS
 * - captures the screen with getDisplayMedia
 * - forwards WebRTC offers/ICE to viewers
 */

const room = qs("room");
const token = qs("token");

document.getElementById("room").textContent = room || "(missing)";

const infoEl = document.getElementById("info");
const btnStart = document.getElementById("btnStart");
const btnStop = document.getElementById("btnStop");

let ws = null;
const peers = new Map();

let screenStream = null;
let registered = false;
let state = "idle";

function syncNativeStatus() {
  pushHostStatus(state, {
    room,
    viewers: peers.size,
    sharing: !!screenStream,
  });
}

function setState(nextState) {
  state = nextState;
  document.getElementById("state").textContent = nextState;
  syncNativeStatus();
}

function updateViewers() {
  document.getElementById("viewers").textContent = String(peers.size);
  syncNativeStatus();
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
      return;
    }

    if (msg.type === "peer.joined") {
      const peerId = msg.peerId;
      log(`peer joined: ${peerId}`);
      await ensurePeer(peerId);
      updateViewers();
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

  // Consume the current renegotiation request before creating an offer.
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

  const videoTrack = screenStream.getVideoTracks()[0];
  if (videoTrack) {
    videoTrack.onended = () => {
      log("screen track ended");
      stopShare("screen_ended");
    };
  }

  for (const [peerId, peer] of peers) {
    attachStreamToPeer(peer, screenStream);
  }

  setState("sharing");
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
  log(`session ended: ${reason}`);
  setState("ready");
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
  }
};

btnStop.onclick = () => {
  stopShare("host_stopped");
  btnStart.disabled = false;
  btnStop.disabled = true;
};

window.addEventListener("beforeunload", () => {
  try {
    if (registered) wsSend({ type: "session.end", room, token, reason: "host_unloaded" });
  } catch {}
});

if (infoEl) {
  infoEl.textContent =
    "Run on HTTPS. Open this page with /host?room=<ROOM>&token=<TOKEN>, then click Start Share.";
}

setState("idle");
startSignal();
