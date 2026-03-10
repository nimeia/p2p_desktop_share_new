function qs(name) {
  const u = new URL(location.href);
  return u.searchParams.get(name) || "";
}

function postToNative(obj) {
  try {
    if (window.chrome && window.chrome.webview && typeof window.chrome.webview.postMessage === "function") {
      window.chrome.webview.postMessage(JSON.stringify(obj));
    }
  } catch {
    // Ignore when not hosted in WebView2.
  }
}

function log(msg) {
  const text = String(msg ?? "");
  const el = document.getElementById("log");
  const line = `[${new Date().toISOString()}] ${text}\n`;
  if (el) el.textContent += line;
  console.log(text);
  postToNative({ source: "host-page", kind: "log", message: text });
}

function pushHostStatus(state, extra = {}) {
  postToNative({ source: "host-page", kind: "status", state, ...extra });
}

function wssUrl(path) {
  const proto = location.protocol === "https:" ? "wss:" : "ws:";
  return `${proto}//${location.host}${path}`;
}

async function wait(ms) { return new Promise(r => setTimeout(r, ms)); }
