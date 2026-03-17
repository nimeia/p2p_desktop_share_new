# HTTP-First 重构方案

## 目标

这版方案的目标只有四个：

- `viewer` 不安装任何软件，直接用浏览器打开。
- `host` 不再出现证书不安全提示。
- 主流程保持 `引导 -> 准备 -> 共享` 三步，不再跳业务新窗口。
- 删除 HTTPS / 自签证书 / 信任引导带来的认知负担。

这不是对当前方案的小修补，而是一次架构收口：

- `host/admin` 只服务本机操作员。
- `viewer` 只服务局域网接收端。
- 两者不再共用同一个“必须 HTTPS”的前提。

## 核心结论

### 1. `host` 不再走局域网 HTTPS

`host` 的屏幕采集能力依赖“可信来源”，但不一定依赖 HTTPS。

最终方案里：

- `host/admin` 固定走 `http://127.0.0.1:<port>` 或 `http://localhost:<port>`
- 不再对外暴露 `https://<LAN-IP>/host?...`
- 不再让本机操作员处理任何证书提示

### 2. `viewer` 走局域网 HTTP

`viewer` 的目标是“零安装、零学习成本、直接打开”。

最终方案里：

- `viewer` 地址固定为 `http://<hostIp>:<port>/view?room=<room>`
- 页面只保留浏览器观看能力
- 不再依赖 service worker、安装提示、自签证书信任

### 3. Admin Shell 不再用 `file://`

当前主界面是本地文件页，后续会持续带来桥接、状态分裂和安全上下文判断问题。

最终方案里：

- 桌面 WebView 固定加载 `http://127.0.0.1:<port>/admin`
- `guide / prepare / share / advanced` 都在这个本地 HTTP Shell 内完成
- 不再依赖 `file://.../index.html`

### 4. 共享页自己拥有 Host 能力

最终方案里：

- 共享页直接拥有 capture controller
- 点击“开始共享”时直接触发系统共享选择器
- 不再通过隐藏 `/host` iframe 控制
- `/host` 页面仅保留为调试/兼容入口，最终可以降级为 legacy 页面

## 目标架构

### 运行时角色

建议拆成两个访问面，但共用同一个房间状态和 WebRTC Hub：

- 本机控制入口：`127.0.0.1:<port>`
- 局域网分享入口：`<hostIp>:<port>`

两者共用：

- 同一份 room/token
- 同一份 viewer 计数
- 同一份 WebRTC / signaling hub
- 同一份健康状态与诊断结果

### 监听策略

推荐方案是同端口双监听：

- 一个 listener 绑定 `127.0.0.1:<port>`
- 一个 listener 绑定当前选中的 `hostIp:<port>`

这样可以同时满足：

- 本机 `host/admin` 始终走 loopback
- 远端 `viewer` 始终走 LAN IP
- URL 简单，用户只看到一个 viewer 端口
- 不必把服务暴露到所有网卡

不推荐继续保留当前单一 TLS listener。

## 统一路由

建议最终保留以下路由：

- `/admin`
- `/admin/share`
- `/admin/advanced`
- `/view`
- `/ws`
- `/health`
- `/api/status`
- `/assets/*`

说明：

- `/admin` 只给桌面 WebView 使用
- `/view` 只给接收方浏览器使用
- `/ws` 继续作为 signaling 通道，但挂在 HTTP 体系下的 `ws://`
- `/host` 可以短期保留，长期应并入 `/admin/share`

## 最终用户流程

### 1. 启动后进入引导页

只问一句话：

`接收方设备现在和这台电脑连的是同一个 Wi-Fi / 路由器吗？`

三个选项：

- `是，在同一个网络`
- `否，使用这台电脑开热点`
- `我不确定`

### 2. 进入准备页

准备页不暴露后台术语，只显示阶段感：

- `正在检查网络`
- `正在选择共享地址`
- `正在启动共享服务`
- `正在生成连接信息`
- `准备完成`

这里不再出现任何证书相关提示。

### 3. 进入共享页

共享页只保留三个主动作：

- `开始共享`
- `停止共享`
- `更换共享窗口`

共享页内直接触发：

- `getDisplayMedia()`
- WebRTC 推流
- viewer 连接状态刷新

不再弹出新的业务窗口。

### 4. 接收方加入

共享页始终展示：

- `Viewer URL`
- 二维码
- 当前连接方式
- 热点名称和密码（如果是热点模式）
- 已连接设备数

接收方只做一件事：

- 打开 `http://<hostIp>:<port>/view?room=<room>`

## 页面级目标

### Guide

目标：

- 让用户只做“选网络方式”这一件事

禁止出现：

- IP
- 端口
- room
- token
- bind
- 证书

### Prepare

目标：

- 把复杂准备动作包装成一个系统自动过程

成功条件改为：

- 服务已启动
- 本机控制入口可用
- viewer 入口已生成
- 局域网或热点路径已就绪

不再把 `certReady` 当作成功条件。

### Share

目标：

- 让用户一眼知道“现在能不能开始分享”

建议只保留五种用户状态：

- `准备中`
- `等待选择共享内容`
- `正在共享`
- `已有设备连接`
- `需要修复`

### Advanced

目标：

- 保留专业能力，但完全退出主流程

保留：

- 网卡切换
- 端口状态
- 防火墙诊断
- 原始日志
- bundle / report 导出

移除：

- 证书生成
- 证书信任
- 证书诊断

## 与当前实现相比要删除的东西

以下能力会被明确降级或删除：

- 自签证书生成
- trust-local-certificate
- WebView2 证书错误放行逻辑
- handoff 中所有“接受本地证书告警”的文案
- viewer 的安装提示和证书说明
- 共享页对隐藏 `/host` iframe 的依赖
- `file://` Admin Shell

## 需要新增或重构的底层能力

### 1. Plain HTTP / WS 服务栈

当前服务端是 TLS-only，需要补 plain HTTP / WS 版本。

至少要重构这些层：

- listener
- http session
- websocket session
- service host
- health probe
- URL builder

### 2. `/admin` 路由

当前桌面壳层加载的是本地文件页，后续要改成：

- WebView 导航到 `/admin`
- WebUI 静态资源由本地服务提供
- Admin Shell 与 Share Runtime 统一到同一个应用状态

### 3. Host Controller 内聚到 Share 页

当前 `www/assets/app_host.js` 的能力需要拆成可复用模块：

- signaling lifecycle
- capture lifecycle
- peer lifecycle
- telemetry pushback
- start / stop / choose share

最终由 `/admin/share` 直接调用，而不是由独立页面承载。

### 4. 健康检查改为 HTTP 语义

当前很多健康检查仍然写死为 `https://.../health`。

后续要统一改成：

- `http://127.0.0.1:<port>/health`
- `http://<hostIp>:<port>/health`

同时清理：

- 证书就绪检查
- 证书 SAN 检查
- 证书导入/信任提示

## 推荐实施顺序

### 阶段 1

先把底层传输从 HTTPS/WSS 改成 HTTP/WS，但保留现有页面结构。

目标：

- 去掉证书
- 让 host 能通过 `127.0.0.1` 正常启动
- 让 viewer 能通过 LAN HTTP 打开

### 阶段 2

把桌面主界面从 `file://` 切到 `/admin`。

目标：

- WebView 直接跑本地 HTTP Shell
- 共享页与运行时状态合一

### 阶段 3

把 `host` 控制逻辑并入 `/admin/share`。

目标：

- 删除隐藏 iframe
- 删除自动弹窗 fallback
- 点击“开始共享”直接弹系统选择器

### 阶段 4

清理历史 HTTPS 残留。

目标：

- 删除证书相关 UI / 文案 / 诊断
- 删除 legacy `/host` 依赖

## 风险与验证

这版方案最需要提前验证的一点不是 `host`，而是 `viewer` 浏览器矩阵。

上线前必须实测：

- Windows Chrome / Edge
- Android Chrome
- iPhone Safari
- iPad Safari

重点验证：

- HTTP viewer 能否稳定收流
- `ws://` signaling 是否被目标浏览器放行
- 自动播放提示是否可接受
- 横屏/全屏体验是否足够顺滑

如果浏览器矩阵验证通过，这版方案会明显比当前 HTTPS MVP 更适合你的产品目标：

- 更少学习成本
- 更少技术术语
- 更少“为什么这里不安全”的质疑
- 更少窗口切换
- 更强的一键感

