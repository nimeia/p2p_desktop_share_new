#include "pch.h"
#include "WebViewShellAdapter.h"

#include "WebViewHost.h"

#include <algorithm>

namespace fs = std::filesystem;

namespace lan::desktop {

std::wstring BuildWebViewFileUrl(const fs::path& path) {
    std::wstring value = fs::absolute(path).wstring();
    std::replace(value.begin(), value.end(), L'\\', L'/');

    std::wstring encoded;
    encoded.reserve(value.size() + 16);
    for (wchar_t ch : value) {
        switch (ch) {
        case L' ':
            encoded += L"%20";
            break;
        case L'#':
            encoded += L"%23";
            break;
        case L'%':
            encoded += L"%25";
            break;
        default:
            encoded.push_back(ch);
            break;
        }
    }

    return L"file:///" + encoded;
}

bool EnsureWebViewShellInitialized(WebViewHost& webview,
                                   const WebViewShellContext& context,
                                   WebViewShellHooks hooks) {
    if (!context.parent) {
        return false;
    }

    return webview.Initialize(
        context.parent,
        context.bounds,
        std::move(hooks.log),
        std::move(hooks.webMessage));
}

WebViewShellPlan BuildWebViewHostNavigationPlan(const WebViewShellState& state,
                                                const WebViewShellContext& context) {
    WebViewShellPlan plan;
    plan.nextState = state;
    plan.nextState.surface = WebViewShellSurface::HostPreview;
    plan.nextState.htmlAdminNavigated = false;
    plan.ensureInitialized = true;
    plan.navigate = !context.hostPreviewUrl.empty();
    plan.navigateUrl = context.hostPreviewUrl;
    if (!context.hostPreviewUrl.empty()) {
        plan.logLine = L"Embedded host page navigate: " + context.hostPreviewUrl;
    }
    return plan;
}

WebViewShellPlan BuildWebViewHtmlAdminNavigationPlan(const WebViewShellState& state,
                                                     const WebViewShellContext& context) {
    WebViewShellPlan plan;
    plan.nextState = state;
    plan.nextState.surface = WebViewShellSurface::HtmlAdminPreview;
    plan.nextState.adminShellReady = false;
    plan.ensureInitialized = true;

    if (!context.htmlAdminIndexFile.empty() && fs::exists(context.htmlAdminIndexFile)) {
        plan.nextState.htmlAdminNavigated = true;
        plan.navigate = true;
        plan.navigateUrl = BuildWebViewFileUrl(context.htmlAdminIndexFile);
        plan.logLine = L"HTML admin shell navigate: " + context.htmlAdminIndexFile.wstring();
    } else {
        plan.nextState.htmlAdminNavigated = false;
        plan.logLine = L"HTML admin shell missing: " + context.htmlAdminIndexFile.wstring();
    }

    return plan;
}

WebViewShellPlan BuildWebViewRestorePlan(const WebViewShellState& state,
                                         const WebViewShellContext& context) {
    switch (state.surface) {
    case WebViewShellSurface::HostPreview:
        return BuildWebViewHostNavigationPlan(state, context);
    case WebViewShellSurface::HtmlAdminPreview: {
        WebViewShellPlan plan;
        plan.nextState = state;
        plan.ensureInitialized = true;
        if (!state.htmlAdminNavigated) {
            return BuildWebViewHtmlAdminNavigationPlan(state, context);
        }
        return plan;
    }
    case WebViewShellSurface::Hidden:
    default:
        return {};
    }
}

void ApplyWebViewShellPlan(WebViewHost& webview,
                           WebViewShellState& state,
                           const WebViewShellPlan& plan,
                           const WebViewShellContext& context,
                           WebViewShellHooks hooks) {
    state = plan.nextState;

    if (plan.ensureInitialized) {
        EnsureWebViewShellInitialized(webview, context, WebViewShellHooks{hooks.log, hooks.webMessage});
    }
    if (plan.navigate && !plan.navigateUrl.empty()) {
        webview.Navigate(plan.navigateUrl);
    }
    if (!plan.logLine.empty() && hooks.log) {
        hooks.log(plan.logLine);
    }
}

} // namespace lan::desktop
