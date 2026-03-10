# Desktop Host 集成说明（占位）

本包只提供后端服务与网页骨架。桌面宿主项目建议做：
- Network Wizard（热点/Wi‑Fi Direct 选择与引导）
- Start Service（启动 ServiceHost）
- Open WebView2 to host URL
- Share Card（SSID/密码/URL/二维码）
- Viewer list/status（观众数、状态）

WebView2 导航示例：
- Host: https://{hostIp}:{port}/host?room={roomId}&token={hostToken}
- Viewer: https://{hostIp}:{port}/view?room={roomId}
