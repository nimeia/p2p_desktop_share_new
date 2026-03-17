#include "pch.h"
#include "WebViewShellAdapter.h"

#include "WebViewHost.h"

namespace lan::desktop {

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

    if (!context.adminShellUrl.empty()) {
        plan.nextState.htmlAdminNavigated = true;
        plan.navigate = true;
        plan.navigateUrl = context.adminShellUrl;
        plan.logLine = L"HTML admin shell navigate: " + context.adminShellUrl;
    } else {
        plan.nextState.htmlAdminNavigated = false;
        plan.logLine = L"HTML admin shell URL is empty";
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
