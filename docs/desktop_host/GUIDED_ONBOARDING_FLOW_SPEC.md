# 引导页与简化共享页交互规格

## 目标

把当前偏后台工作台式的桌面端入口，收敛成一个新手无需学习即可完成的前台流程。

目标用户行为只有三步：

1. 选择连接方式
2. 等待系统自动准备
3. 选择共享内容并开始共享

本规格优先复用现有桌面宿主、HTML Admin Shell、服务启动、热点控制、网络诊断、分享材料导出能力，不新增独立原生窗口。

## 当前实现基础

当前代码已经具备以下可复用能力：

- 本地服务启动、停止、状态轮询
- 自动生成 room / token
- 局域网 IPv4 探测、候选网卡排序、手动切换
- 热点配置、热点启动 / 停止、系统热点设置兜底
- 证书状态、Windows 防火墙、LAN 可达性、自检诊断
- Viewer URL、二维码 / Share Card、Share Wizard
- host 页面状态回传、viewer 数量回传、托盘快捷操作

对应代码入口：

- `src/desktop_host/MainWindow.cpp`
- `src/desktop_host/webui/index.html`
- `src/desktop_host/webui/app.js`
- `www/assets/app_host.js`
- `src/core/runtime/shell_bridge_presenter.h`

## 当前产品边界

本规格必须明确以下边界，否则交互文案会与真实能力不一致：

- 当前“选择共享程序”本质上是触发浏览器 / WebView2 的 `getDisplayMedia` 选择器，让用户选择窗口、应用窗口或整个屏幕，不是桌面端原生程序列表。
- 当前桌面壳层可以启动服务、打开 host 页，但还不能直接命令 host 页执行“开始共享”。
- 当前 host 页回传给桌面端的主要是 `state` 和 `viewers`，还没有“当前共享目标名称”字段。
- 当前设置页不是持久化配置中心，网络方式、上次热点配置、上次引导选择默认不会跨启动保留。

## 设计原则

- 首页只问一个问题，不展示端口、token、bind、诊断术语。
- 每个页面只有一个主任务。
- 每个失败状态只给一个主修复动作。
- 把“服务准备”和“开始共享”拆开，但不要把这个复杂性暴露给用户。
- 连接给其他人的信息必须始终可见。
- 高级日志、诊断、原始状态全部收进“高级诊断”入口。

## 推荐信息架构

简化模式表面只有三个页面：

1. 引导页
2. 自动准备页
3. 共享页

同时保留一个隐藏的高级入口：

- 高级诊断 / 高级设置

建议第一期仍然复用当前 HTML Admin Shell，只是在 WebUI 中增加一个 simple route，而不是新增原生窗口。

建议增加以下 simple route：

- `guide`
- `prepare`
- `share`
- `advanced`

## 页面一：引导页

### 页面目标

帮助用户用非技术语言选择“同网共享”还是“本机开热点共享”。

### 推荐文案

标题：

- `接收方设备现在和这台电脑连的是同一个 Wi‑Fi / 路由器吗？`

选项：

- `是，在同一个网络`
- `否，使用这台电脑开热点`
- `我不确定`

辅助文案：

- `同一个网络：系统会自动准备共享地址。`
- `开热点：系统会先配置热点，再生成连接信息。`
- `不确定：系统先尝试同网共享，不行再引导你开热点。`

### 页面元素

- 标题
- 一句解释
- 3 个选项卡片
- 一个次级入口：`高级设置`

### 交互规则

- 选择“同一个网络”后进入 `prepare(lan)`
- 选择“使用本机开热点”后进入 `prepare(hotspot)`
- 选择“我不确定”后进入 `prepare(auto)`
- 页面默认不展示 IP、端口、room、token

### 需要复用的现有能力

- `refresh-network`
- 候选网卡推荐逻辑
- 自动生成 room / token

### 需要补充的细节

- 是否记住用户上次选择
- 是否把“我不确定”记成默认路径
- 若多网卡并存，是否直接采用推荐网卡而不打断用户

## 页面二：自动准备页

### 页面目标

在不暴露后台概念的前提下，把“网络检查 + 热点处理 + 服务启动 + 可达性检查”包装成一段可理解的进度流程。

### 页面结构

- 大状态标题
- 当前步骤文案
- 进度条或步骤列表
- 当前连接方式摘要
- 一个主按钮区域
- 一个问题修复区域

### 同网路径步骤

建议步骤固定为：

1. `正在检测网络`
2. `正在选择共享地址`
3. `正在启动共享服务`
4. `正在生成连接信息`
5. `准备完成`

内部实际动作建议对应：

1. 刷新网络状态
2. 自动选中推荐网卡
3. 自动补全 room / token
4. 启动服务
5. 读取 `serverRunning / healthReady / hostReachable / certReady`

### 热点路径步骤

建议步骤固定为：

1. `正在生成热点信息`
2. `正在启动热点`
3. `正在启动共享服务`
4. `正在生成连接信息`
5. `准备完成`

页面必须显示：

- 热点名称
- 热点密码
- 一个可见提示：`请让接收方先连接这个热点，再扫码或打开地址`

### 自动路径步骤

建议逻辑：

1. 先走同网路径
2. 如果没有可用 LAN 地址、可达性失败、或当前网络明显不适合交付，则转为热点引导
3. 不要静默切换，文案必须提示：`未检测到稳定的同网路径，建议改用本机热点`

### 成功判定

对于简化模式，不能把“进程已启动”直接当作“已准备好”。

建议定义为：

- `serverRunning == true`
- `healthReady == true`
- `certReady == true`
- 满足下列之一：
- `hostReachable == true`
- `hotspotRunning == true`

如果防火墙或远端可达性仍然存在阻塞，不应显示最终成功，应显示：

- `还差一步`
- 主修复动作：`允许网络访问` 或 `重新检测网络`

### 失败态设计

简化模式不进入复杂页签，保留一行问题说明和一个主修复按钮即可。

建议覆盖的失败态：

- `未检测到可用网络`
- `热点启动失败`
- `当前机器不支持直接控制热点`
- `服务启动失败`
- `端口被占用`
- `证书未准备好`
- `Windows 防火墙可能阻止其它设备访问`

### 失败态按钮映射

- `重新检测网络`
- `打开系统热点设置`
- `允许网络访问`
- `重新启动服务`
- `打开高级诊断`

### 需要复用的现有字段

- `serverRunning`
- `healthReady`
- `hostReachable`
- `certReady`
- `certDetail`
- `firewallReady`
- `firewallDetail`
- `remoteViewerReady`
- `remoteViewerDetail`
- `hotspotRunning`
- `hotspotSupported`
- `hotspotStatus`
- `hostIp`
- `networkCandidates`

### 需要复用的现有命令

- `refresh-network`
- `select-adapter`
- `start-server`
- `apply-hotspot`
- `start-hotspot`
- `open-hotspot-settings`
- `open-firewall-settings`
- `run-network-diagnostics`
- `trust-local-certificate`

## 页面三：共享页

### 页面目标

用户在这个页面只做一件事：让共享真正开始，并把连接方式交给对方。

### 页面布局

上区：

- 大状态条
- 一句当前引导

中区：

- 3 个主操作

下区：

- 对方连接卡片
- 当前连接方式卡片
- 已连接设备状态

### 主操作定义

保留用户提出的三项，但建议文案微调：

- `开始共享`
- `停止共享`
- `选择共享窗口`

说明：

- 若继续使用“选择共享程序”，会让用户误以为桌面端能原生列出进程；当前实现并非如此。

### 三个按钮的推荐行为

#### 开始共享

用户心智应该是“一键开始”，内部可以动态处理：

- 若服务未准备：先执行准备流程
- 若服务已准备但未开始捕获：触发 host 页开始共享
- 若正在共享：按钮隐藏或禁用

#### 停止共享

建议在简化模式里定义为“结束本次共享”：

- 停止当前捕获
- 结束当前会话
- 停止服务

原因：

- 新手不需要区分“停止画面共享”和“停止服务”
- 安全感更强
- 状态更清楚

#### 选择共享窗口

建议行为：

- 若未共享：打开系统内容选择器
- 若正在共享：允许重新选择共享目标

### 状态条文案

建议只保留四种用户态：

- `准备中`
- `等待选择共享内容`
- `正在共享`
- `已有设备连接`

异常时替换为：

- `需要修复`

### 连接卡片必须展示的内容

即使不作为主按钮，也必须始终可见：

- 二维码
- Viewer URL
- 当前路径是“同网共享”还是“热点共享”
- 如果是热点，显示 SSID / 密码
- 已连接设备数

### 共享页不能缺少的反馈

- `0 台设备已连接`
- `1 台设备已连接`
- `共享已停止`
- `连接已断开，可重新开始共享`

### 需要复用的现有字段

- `viewerUrl`
- `viewers`
- `networkMode`
- `hotspotSsid`
- `hotspotPassword`
- `hotspotRunning`
- `handoffState`
- `handoffLabel`
- `handoffDetail`
- `hostState`

### 需要复用的现有命令

- `show-qr`
- `copy-viewer-url`
- `show-share-wizard`
- `stop-server`

## 高级入口

### 目标

把专业能力留下来，但不影响新手主流程。

### 建议入口名称

- `高级诊断`

### 建议收纳内容

- 网卡切换
- 端口、bind、room、token
- 防火墙诊断
- 证书诊断
- 原始日志
- 导出 bundle / report
- Settings / Diagnostics / Monitor 全部页签

## 简化状态机

### 顶层状态

- `guide-question`
- `prepare-lan`
- `prepare-hotspot`
- `prepare-auto`
- `share-ready`
- `sharing-active`
- `viewer-connected`
- `blocked`
- `stopped`

### 状态含义

#### guide-question

刚启动，尚未选择路径。

#### prepare-lan

用户已选同网，系统正在检测网络并准备服务。

#### prepare-hotspot

用户已选热点，系统正在准备热点并启动服务。

#### prepare-auto

用户不确定，系统先尝试同网，再决定是否切换到热点建议。

#### share-ready

服务可用，等待用户选择共享内容。

#### sharing-active

共享内容已选定并开始推流，但还没有 viewer 连接。

#### viewer-connected

至少有一台 viewer 已连接。

#### blocked

当前存在阻塞性问题，主流程不能继续。

#### stopped

用户主动结束共享，回到一个清晰可再次开始的空状态。

### 核心转移

- `guide-question -> prepare-lan`
- `guide-question -> prepare-hotspot`
- `guide-question -> prepare-auto`
- `prepare-* -> share-ready`
- `prepare-* -> blocked`
- `share-ready -> sharing-active`
- `sharing-active -> viewer-connected`
- `viewer-connected -> sharing-active`
- `sharing-active -> stopped`
- `viewer-connected -> stopped`
- `blocked -> prepare-lan`
- `blocked -> prepare-hotspot`
- `stopped -> share-ready`

## 与现有状态字段的映射建议

### 顶层简化状态映射

建议优先复用已有字段，不另起一套复杂诊断模型。

#### share-ready

满足：

- `serverRunning`
- `healthReady`
- `certReady`
- `hostState` 不是 `sharing`

#### sharing-active

满足：

- `hostState == sharing`
- `viewers == 0`

#### viewer-connected

满足：

- `hostState == sharing`
- `viewers > 0`

#### blocked

满足任一条件：

- 服务启动失败
- `healthReady == false`
- `certReady == false`
- 同网模式下 `hostReachable == false`
- 需要热点但 `hotspotRunning == false`
- 已知防火墙阻塞且远端路径不可用

### 用户态文案可直接复用的现有模型

当前已有一套 handoff state：

- `not-started`
- `ready-for-handoff`
- `delivered`
- `needs-fix`

简化模式可以继续用这套状态做底层依据，只把用户文案改成更口语化版本。

## 需要新增的桥接点

这是让“共享页真正只剩三个动作”成立的关键。

### 必须新增

#### 1. 桌面页触发 host 页开始共享

需要新增从桌面端发送到 host 页的命令，例如：

- `host-start-share`

host 页收到后执行：

- 调用 `getDisplayMedia`
- 成功后回传状态

#### 2. 桌面页触发 host 页停止共享

需要新增：

- `host-stop-share`

host 页收到后执行当前 stopShare 逻辑。

#### 3. host 页回传共享目标名称

需要新增回传字段，例如：

- `captureLabel`

用于共享页显示：

- `当前共享：微信`
- `当前共享：Excel`
- `当前共享：整个屏幕`

#### 4. host 页回传更细的内容选择状态

建议新增：

- `captureState`

可取值：

- `not-started`
- `selecting`
- `selected`
- `ended`
- `error`

### 可选新增

#### 5. 记住上次连接方式

建议持久化：

- 上次选择的是同网还是热点
- 上次热点名称
- 上次热点密码
- 上次选中的网卡 IP

#### 6. 自动处理端口占用

当前已有端口可用性判断，但简化模式更适合：

- 自动换到下一个可用端口
- 再继续准备流程

## 第一阶段实现建议

为了尽快落地，建议分两段交付。

### 第一阶段

目标：

- 做出新手可用的引导流程
- 最大化复用已有命令和状态

做法：

- 在 HTML Admin Shell 内增加 `guide / prepare / share` 三个 simple route
- 默认启动进入 `guide`
- 准备页基于现有 snapshot 字段判断成败
- 共享页先展示二维码、Viewer URL、热点信息、viewer 数量
- “开始共享”第一阶段可先退化为：
- 打开或聚焦 host 页，并引导用户点击 host 页里的开始按钮

### 第二阶段

目标：

- 让共享页真正完成“一键开始共享 / 一键停止共享 / 一键切换共享目标”

做法：

- 补桌面页到 host 页的控制桥
- 补 capture 状态和 captureLabel 回传
- 让简化页不再依赖用户点击 host 页内部按钮

## 页面文案样例

### 引导页

- `接收方设备现在和这台电脑连的是同一个 Wi‑Fi / 路由器吗？`
- `是，在同一个网络`
- `否，使用这台电脑开热点`
- `我不确定`

### 自动准备页

- `正在准备共享环境`
- `正在检测网络`
- `正在启动共享服务`
- `正在生成连接信息`
- `准备完成`
- `还差一步`

### 共享页

- `等待选择共享内容`
- `开始共享`
- `停止共享`
- `选择共享窗口`
- `让对方扫码或打开这个地址`
- `0 台设备已连接`
- `1 台设备已连接`
- `需要修复`

## 验收标准

如果以下几点能成立，这个简化流程就达到了“专业但易用”的目标：

- 用户首次打开时，不需要理解 room、token、bind、端口
- 用户能在 10 秒内选定同网或热点路径
- 用户能在 20 秒内看到清晰的二维码或连接地址
- 用户能在共享页明确知道“还没开始共享”与“已经开始共享”的区别
- 用户在异常时只会看到一个主修复动作，而不是一组后台信息
- 高级诊断仍可进入，但不会打断主路径
