# Microsoft Store Certification Notes

This file is intended for the Partner Center field:

`Notes for certification`

Users do not see this information.

## Paste-ready English version

This submission is a Windows desktop host for local-network screen sharing.

Key points for certification:

- No account, sign-in, subscription, payment flow, or external service credentials are required.
- The app does not depend on a public cloud backend for its core scenario.
- The product is intended for same-LAN / local-hotspot use only. It is not an internet-sharing product.
- The package uses local HTTP/WS service endpoints on localhost/private network to serve the host/viewer pages and handle signaling.
- The app requests private-network access because viewer devices may connect from the same local network.

How to test the primary scenario:

1. Launch `ViewMesh`.
2. Start the local sharing service from the desktop UI.
3. Open the Host page from the app. The embedded Host/Admin experience uses WebView2 when available.
4. If WebView2 is unavailable in the certification environment, browser fallback is supported. Please open the Host page in an external browser from the app and continue testing there.
5. When prompted, grant screen-capture permission. The current MVP uses screen/window/tab capture only. Camera and microphone are not required for the main scenario.
6. Open the Viewer page from the app, or open the generated Viewer URL in a second browser window/tab on the same machine, or on another device on the same LAN.
7. Confirm that the viewer connects and the shared screen is visible on the Viewer page. The desktop UI should also reflect room/viewer status.

Additional notes:

- No hidden features require unlock codes or test credentials.
- The core validation path is: app launch, local service startup, Host page open, screen-capture permission grant, Viewer page open, local signaling, and basic viewer playback.
- The app supports one host and multiple viewers in the same room.
- Current transport is local HTTP / WS and WebRTC-based media transport.

Known environment note:

- WebView2 is optional. If the embedded preview is unavailable in the certification machine, external-browser fallback is an intended supported path for this submission.

Known packaging note:

- In the current build lineage, some diagnostics/share-bundle export paths still need writable-location hardening for installed MSIX environments. These export-oriented flows are not required for the primary launch-and-share validation path above.

## Chinese reference version

本提交是一个面向局域网屏幕共享的 Windows 桌面 Host 应用。

审核说明要点：

- 不需要账号、登录、订阅、支付流程或外部服务测试凭据。
- 核心功能不依赖公网云服务。
- 产品定位是同一局域网 / 本地热点内共享，不是公网远程共享产品。
- 应用会在本机启动本地 HTTP/WS 服务，用于提供 Host / Viewer 页面和信令。
- 之所以需要私有网络访问能力，是因为 Viewer 可能从同一局域网中的其他设备连接。

建议审核路径：

1. 启动 `ViewMesh`。
2. 在桌面界面中启动本地共享服务。
3. 从应用中打开 Host 页面。若环境支持，则会优先使用 WebView2 嵌入式体验。
4. 如果审核环境没有 WebView2，也可以从应用中改为使用外部浏览器打开 Host 页面继续测试。
5. 当浏览器或 WebView2 请求屏幕捕获权限时，请授予权限。当前 MVP 主要使用屏幕/窗口/标签页捕获，不要求摄像头和麦克风。
6. 从应用中打开 Viewer 页面，或在同一台机器的第二个浏览器窗口/标签页中打开生成的 Viewer URL，也可以在同一局域网的另一台设备中打开该 URL。
7. 验证 Viewer 连接成功并能看到共享画面，同时桌面 UI 中的 room / viewer 状态会更新。

补充说明：

- 没有隐藏功能需要解锁码或测试账号。
- 本次提交的主要验证路径是：应用启动、本地服务启动、Host 页面打开、屏幕捕获授权、Viewer 页面打开、本地信令建立和基本播放。
- 产品支持同一房间 1 个 Host、多个 Viewer。
- 当前版本使用本地 HTTP / WS 和基于 WebRTC 的媒体传输。

环境说明：

- WebView2 是可选能力。如果审核机没有 WebView2，使用外部浏览器回退路径属于预期支持场景。

当前已知说明：

- 当前构建链路下，部分 diagnostics / share bundle 导出路径在已安装 MSIX 环境中仍需进一步切换到可写目录。这些导出类功能不是本次主验证路径的前置条件。

