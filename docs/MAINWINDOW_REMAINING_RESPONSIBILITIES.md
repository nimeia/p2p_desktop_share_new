# MainWindow.cpp Remaining Responsibilities Review (Iter 24)

## Scope

This review does **not** try to extract another shared runtime layer yet. It audits the current `src/desktop_host/MainWindow.cpp` after Iter 1-23 and classifies the remaining responsibilities into three buckets:

1. already thin enough,
2. still worth one more extraction,
3. should stay in the Win32 shell.

## Current snapshot

- `src/desktop_host/MainWindow.cpp`: **3339** lines
- `src/desktop_host/MainWindow.h`: **441** lines
- `MainWindow.cpp` methods detected: **97**

The file is no longer dominated by session/runtime/business-state derivation. Most of that has already moved into shared runtime layers.

## What is already successfully extracted

The following categories are no longer the main problem in `MainWindow.cpp`:

- certificate inspection and generation split
- network endpoint selection and platform probes split
- runtime snapshot shaping
- share bundle / diagnostics export shaping
- platform quick-fix / system action facade
- start/stop/open/export action coordination
- session/config normalization and admin sync
- admin/dashboard/settings/diagnostics/shell-fallback view-model assembly
- native command routing and button policy
- edit draft / dirty / pending-apply policy
- tray/shell chrome text and tray menu model
- page layout / surface mode / page visibility policy
- shell bridge message parsing and snapshot publishing
- admin shell command dispatch
- observability aggregation
- periodic scheduler policy
- lifecycle coordination

That means the remaining size of `MainWindow.cpp` is now mostly **Win32 shell glue**, not shared business logic.

## Largest remaining hotspots

These are the main remaining high-weight functions by size and by responsibility.

### 1. `OnCreate()` (~476 lines)

This is still the single largest function by far.

What it still owns:

- Win32 control creation
- native page control tree wiring
- combo/list/button initialization
- `ServerController` and `AdminBackend` object setup
- callback wiring into `PostMessageW`
- shell startup sequence handoff

Assessment:

- **Not a shared-runtime problem anymore.**
- Still too large for maintainability.
- Best next extraction target is **Win32-only control tree / page builder**, not another shared runtime module.

Recommended extraction:

- `desktop_host/native_control_factory.*`
- or `desktop_host/page_builders/*.cpp`

### 2. `ExecuteDesktopShellCommand()` (~140 lines)

The routing policy is already shared, but the function still applies many route effects directly.

What remains inside:

- page navigation dispatch
- retry shell plumbing
- copy/save/open helpers
- message box for pairing help
- direct calls into action/session/runtime wrappers

Assessment:

- Policy is already extracted.
- Remaining content is mostly **Win32 shell effect application**.
- Worth one final extraction if you want a thinner shell class.

Recommended extraction:

- `desktop_host/shell_effect_executor.*`

### 3. `ApplyDesktopEditSessionViewModel()` (~137 lines)

This function is now mostly control binding.

What it does:

- writes presenter output into Win32 controls
- updates button labels
- updates summary/status text
- updates visibility/enabled states for setup-page controls

Assessment:

- This is **page binder code**, not shared runtime logic.
- It is acceptable to keep it in the shell, but it could be moved into a Win32-only setup-page binder if you want smaller files.

Recommended extraction:

- `desktop_host/setup_page_binder.*`

### 4. `TrySetPageFromAdminTab()` (~118 lines)

This is now mostly a mapping table from admin tab names to native pages.

Assessment:

- Low architectural value.
- Can be simplified, but it does **not** justify a new shared layer.

Recommended action:

- flatten into a small lookup table / helper
- do **not** spend a full iteration on it

### 5. `ApplyHostShellLifecyclePlan()` (~84 lines)

The lifecycle coordinator is already extracted; this function now applies the plan to Win32 APIs.

What remains inside:

- `ShowWindow`, `UpdateWindow`, `SetForegroundWindow`
- `SetTimer`, `KillTimer`
- `DestroyWindow`
- tray icon create/remove/update
- calls back into refresh methods

Assessment:

- This is now a classic **native shell effect executor**.
- Worth extracting only if you want `MainWindow` to become a very thin façade.
- Otherwise acceptable to keep.

### 6. `HandleAdminShellMessage()` (~81 lines)

Now mostly bridge glue:

- pass payload to `AdminBackend`
- apply refresh publish policy
- publish runtime snapshot
- refresh shell fallback

Assessment:

- Already thin enough.
- No urgent extraction value.

### 7. Page refresh methods

Examples:

- `RefreshNetworkPage()` (~61)
- `RefreshShellFallback()` (~58)
- `RefreshSharingPage()` (~49)
- `RefreshSessionSetup()` (~50)
- `RefreshDashboard()` (~34)
- `RefreshSettingsPage()` (~30)

Assessment:

- Most of the **text derivation and decision policy is already shared**.
- The remaining code is mostly Win32 binding and page-specific control filling.
- These methods are no longer the main architectural risk.

Recommended extraction:

- only if you want page-specific binder files for readability
- not needed for cross-platform runtime extraction

### 8. Platform/file/open helpers

Examples:

- `OpenOutputFolder()` (~55)
- `StartHotspot()` (~43)
- `StopHotspot()`
- `WriteShareArtifacts()` (~39)
- `OnSize()` (~35)

Assessment:

- These are shell/platform operations and OS integration points.
- They should mostly stay in the host shell or in Win32-only helper files.

## What should stay in the Win32 shell

These responsibilities are now correctly shell-local and do **not** need to move into shared runtime:

- `HWND` ownership and lifetime
- `WndProc` / Win32 message dispatch
- native control creation
- `ShowWindow` / `DestroyWindow` / `SetForegroundWindow`
- `Shell_NotifyIconW`, tray menu, balloon interactions
- clipboard access and `MessageBoxW`
- WebView2 controller/view lifetime
- `SetWindowTextW`, `EnableWindow`, `ShowWindow` on individual controls
- `PostMessageW`-based async hops back to UI thread
- Win32 timer registration and raw window events

In other words, the file should still own **native shell mechanics** even after the shared extraction work is done.

## What is still worth extracting

These are the last extractions with a reasonable payoff.

### A. Win32 control tree / page builders

Why:

- biggest readability win
- directly attacks `OnCreate()`
- no protocol/runtime risk

Suggested shape:

- `desktop_host/native_control_factory.*`
- `desktop_host/page_builders/dashboard_page_builder.*`
- `desktop_host/page_builders/setup_page_builder.*`
- ...

Priority: **high**

### B. Shell effect executor

Why:

- reduces `ExecuteDesktopShellCommand()` and `ApplyHostShellLifecyclePlan()`
- creates a cleaner split between **shared plan** and **Win32 effect application**

Suggested shape:

- `desktop_host/shell_effect_executor.*`

Priority: **medium-high**

### C. WebView shell adapter / preview host adapter

Current shell-local methods still form a mini-cluster:

- `EnsureWebViewInitialized()`
- `NavigateHostInWebView()`
- `NavigateHtmlAdminInWebView()`
- `RefreshHtmlAdminPreview()`
- `HandleWebViewMessage()`

Why:

- this cluster is platform-specific but conceptually cohesive
- Linux/macOS hosts will likely need a different embedded-web or browser-host adapter anyway

Suggested shape:

- `desktop_host/web_shell_adapter.*`

Priority: **medium**

### D. Page binder split (optional)

Why:

- makes `MainWindow.cpp` shorter
- little architectural value beyond readability

Suggested shape:

- `desktop_host/page_binders/*.cpp`

Priority: **low-medium**

## What is not worth another shared-runtime extraction

These areas are already in the right place and should not trigger more cross-platform churn right now:

- command policy and button policy
- admin shell message parsing
- runtime snapshot publishing
- diagnostics/dashboard/settings view-model shaping
- session mutation policy
- action coordination
- scheduler policy
- lifecycle decision policy
- observability normalization

A further shared extraction pass here would likely produce diminishing returns and increase complexity.

## Recommended closing sequence

### Iter 25

Extract **Win32 control/page builders** to shrink `OnCreate()`.

### Iter 26

Extract **WebView shell adapter** to isolate embedded preview/admin host integration.

### Iter 27

Extracted **shell effect executor** for lifecycle + command effect application.

After that, freeze `MainWindow.cpp` as the **Windows shell façade** and shift energy to:

- Linux host shell skeleton
- macOS host shell skeleton
- shared runtime reuse validation across hosts

## Bottom line

`MainWindow.cpp` is no longer the wrong place for core runtime policy.

What remains is mostly:

- Win32 shell mechanics,
- control binding,
- embedded web host integration,
- a still-too-large control creation function.

So the final recommendation is:

- **stop extracting shared runtime policy from `MainWindow.cpp`; that job is largely done**,
- do **one or two Win32-only readability extractions**,
- then move on to Linux/macOS host shells.

## Iter 25 落地

本轮已把 `OnCreate()` 中的大块 Win32 控件树初始化拆成：

- `src/desktop_host/NativeControlFactory.*`
- `src/desktop_host/DesktopHostPageBuilders.*`

现在 `MainWindow::OnCreate()` 只保留：

- backend / server 初始化
- 默认 session / path 初始化
- `DesktopHostPageBuilders::BuildAll(*this)`
- startup lifecycle plan 应用

这意味着剩余的 `MainWindow.cpp` 责任更偏向 Win32 壳层动作执行，而不是静态控件树搭建。

## Iter 26 落地

本轮已把 WebView2 的壳层适配进一步拆成：

- `src/desktop_host/WebViewShellAdapter.*`

现在 `MainWindow.cpp` 只保留：

- 构建 WebView shell state/context/hooks
- 调 adapter 执行初始化 / 导航 / restore
- 在 Win32 生命周期中消费 adapter 结果

这意味着“嵌入式 host/admin WebView2 集成”已不再以内联方式散落在窗口类里。

当前更值得做的下一步，已经不是继续拆 WebView 规则，而是可选的：

- Iter 27：shell effect executor（把 command/lifecycle 的 Win32 effect apply 再瘦一层）


## Iter 27 落地

本轮已把 Win32 壳层里的 command / lifecycle effect apply 继续拆成：

- `src/desktop_host/ShellEffectExecutor.*`

现在 `MainWindow.cpp` 只保留：

- 构建 lifecycle / command 输入
- 委托给 `ShellEffectExecutor` 执行 Win32 effect
- 保留少量窗口过程与状态拼装

这意味着 `MainWindow.cpp` 现在更接近真正的 **Windows shell façade**，而不是继续承载大段 Win32 effect apply 代码。
