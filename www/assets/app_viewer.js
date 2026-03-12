/**
 * Viewer page:
 * - joins a room over WSS
 * - receives WebRTC video from the host
 * - falls back to manual play when autoplay is blocked
 */

const room = qs("room");
document.getElementById("room").textContent = room || "(missing)";

const video = document.getElementById("video");
const playHint = document.getElementById("playHint");
const btnPlay = document.getElementById("btnPlay");

video.playsInline = true;
video.autoplay = true;
video.muted = true;
video.defaultMuted = true;

let ws = null;
let peerId = null;
let pc = null;
let playRetryTimer = null;

function setState(nextState) {
  document.getElementById("state").textContent = nextState;
}

function showPlayHint() {
  if (playHint) playHint.style.display = "block";
}

function hidePlayHint() {
  if (playHint) playHint.style.display = "none";
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
    }
  };

  return pc;
}

function cleanupPc() {
  if (playRetryTimer) {
    clearTimeout(playRetryTimer);
    playRetryTimer = null;
  }
  try {
    if (pc) pc.close();
  } catch {}
  pc = null;
  try {
    video.srcObject = null;
  } catch {}
  hidePlayHint();
}

function startSignal() {
  if (!room) {
    log("Missing URL parameter: room. Open /view?room=<ROOM>.");
    setState("config_error");
    return;
  }

  ws = new WebSocket(wssUrl("/ws"));

  ws.onopen = () => {
    log("WSS connected");
    setState("connecting");
    wsSend({ type: "room.join", room });
  };

  ws.onmessage = async (ev) => {
    const msg = JSON.parse(ev.data);

    if (msg.type === "room.joined") {
      peerId = msg.peerId;
      log(`joined as ${peerId}`);
      setState("joined");
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

  ws.onerror = () => log("WSS error");
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
  }
}

video.onloadedmetadata = () => {
  log(`video metadata ${video.videoWidth}x${video.videoHeight}`);
};

video.onplaying = () => {
  log("video playing");
  hidePlayHint();
  setState("playing");
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

document.addEventListener(
  "click",
  () => {
    if (video.srcObject && document.getElementById("state").textContent === "play_blocked") {
      void tryPlayVideo("document-click");
    }
  },
  { passive: true }
);

startSignal();
