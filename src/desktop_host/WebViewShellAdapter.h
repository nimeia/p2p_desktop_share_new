#pragma once

#include <functional>
#include <string>
#include <windows.h>

class WebViewHost;

namespace lan::desktop {

enum class WebViewShellSurface {
    Hidden,
    HostPreview,
    HtmlAdminPreview,
};

struct WebViewShellState {
    WebViewShellSurface surface = WebViewShellSurface::Hidden;
    bool adminShellReady = false;
    bool htmlAdminNavigated = false;
};

struct WebViewShellContext {
    HWND parent = nullptr;
    RECT bounds{};
    std::wstring hostPreviewUrl;
    std::wstring adminShellUrl;
};

struct WebViewShellHooks {
    std::function<void(std::wstring)> log;
    std::function<void(std::wstring)> webMessage;
};

struct WebViewShellPlan {
    WebViewShellState nextState{};
    bool ensureInitialized = false;
    bool navigate = false;
    std::wstring navigateUrl;
    std::wstring logLine;
};

bool EnsureWebViewShellInitialized(WebViewHost& webview,
                                   const WebViewShellContext& context,
                                   WebViewShellHooks hooks);

WebViewShellPlan BuildWebViewHostNavigationPlan(const WebViewShellState& state,
                                                const WebViewShellContext& context);

WebViewShellPlan BuildWebViewHtmlAdminNavigationPlan(const WebViewShellState& state,
                                                     const WebViewShellContext& context);

WebViewShellPlan BuildWebViewRestorePlan(const WebViewShellState& state,
                                         const WebViewShellContext& context);

void ApplyWebViewShellPlan(WebViewHost& webview,
                           WebViewShellState& state,
                           const WebViewShellPlan& plan,
                           const WebViewShellContext& context,
                           WebViewShellHooks hooks);

} // namespace lan::desktop
