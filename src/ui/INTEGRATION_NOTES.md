# Desktop Host Integration Notes

This file is only a lightweight integration placeholder for UI-side wiring.

The desktop shell is expected to drive:

- network wizard / hotspot / Wi-Fi Direct guidance
- start/stop of the local `ServiceHost`
- opening the embedded WebView2 surface when available
- share-card rendering for URL / QR / handoff data
- viewer/status summaries

Current URL examples:

- host: `http://{hostIp}:{port}/host?room={roomId}&token={hostToken}`
- viewer: `http://{hostIp}:{port}/view?room={roomId}`
- admin: `http://127.0.0.1:{port}/admin/`
