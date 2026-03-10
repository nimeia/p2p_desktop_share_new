#include "pch.h"
#include "WebViewHost.h"

#include <wrl.h>
#include <wrl/event.h>

#if __has_include(<WebView2.h>)
#include <WebView2.h>
#define LAN_HAS_WEBVIEW2 1
#else
#define LAN_HAS_WEBVIEW2 0
#endif

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

struct WebViewHost::Impl {
    HWND parent = nullptr;
    RECT bounds{};
    std::function<void(std::wstring)> log;
    std::function<void(std::wstring)> webMessage;
    std::wstring pendingUrl;
    std::wstring status = L"not-initialized";

#if LAN_HAS_WEBVIEW2
    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
#endif

    void Log(const std::wstring& msg) {
        if (log) log(msg);
    }

    void EmitWebMessage(const std::wstring& msg) {
        if (webMessage) webMessage(msg);
    }
};

WebViewHost::~WebViewHost() {
    if (!m_impl) return;

#if LAN_HAS_WEBVIEW2
    m_impl->webview.Reset();
    m_impl->controller.Reset();
    m_impl->env.Reset();
#endif

    delete m_impl;
    m_impl = nullptr;
}

bool WebViewHost::Initialize(HWND parent, const RECT& bounds, std::function<void(std::wstring)> log, std::function<void(std::wstring)> webMessage) {
    if (m_impl) return true;
    m_impl = new (std::nothrow) Impl();
    if (!m_impl) return false;

    m_impl->parent = parent;
    m_impl->bounds = bounds;
    m_impl->log = std::move(log);
    m_impl->webMessage = std::move(webMessage);
    m_impl->status = L"initializing";

#if !LAN_HAS_WEBVIEW2
    m_impl->status = L"sdk-unavailable";
    m_impl->Log(L"WebView2 SDK header not found at build time; embedded host view is disabled.");
    return false;
#else
    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        nullptr,
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (!m_impl) return E_FAIL;
                if (FAILED(result) || !env) {
                    m_impl->status = L"runtime-unavailable";
                    m_impl->Log(L"WebView2: CreateEnvironment failed");
                    return FAILED(result) ? result : E_FAIL;
                }

                m_impl->env = env;
                m_impl->Log(L"WebView2: environment created");

                return m_impl->env->CreateCoreWebView2Controller(
                    m_impl->parent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this](HRESULT result2, ICoreWebView2Controller* controller) -> HRESULT {
                            if (!m_impl) return E_FAIL;
                            if (FAILED(result2) || !controller) {
                                m_impl->status = L"controller-unavailable";
                                m_impl->Log(L"WebView2: CreateController failed");
                                return FAILED(result2) ? result2 : E_FAIL;
                            }

                            m_impl->controller = controller;
                            m_impl->controller->put_Bounds(m_impl->bounds);

                            ComPtr<ICoreWebView2> wv;
                            if (SUCCEEDED(m_impl->controller->get_CoreWebView2(&wv)) && wv) {
                                m_impl->webview = wv;
                            }

                            if (m_impl->webview) {
#if defined(__ICoreWebView2_13_INTERFACE_DEFINED__)
                                ComPtr<ICoreWebView2_13> wv13;
                                if (SUCCEEDED(m_impl->webview.As(&wv13)) && wv13) {
                                    EventRegistrationToken tok{};
                                    wv13->add_ServerCertificateErrorDetected(
                                        Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
                                            [this](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* args) -> HRESULT {
                                                if (!args) return S_OK;
                                                args->put_Action(COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_ALWAYS_ALLOW);
                                                if (m_impl) m_impl->Log(L"WebView2: allowed server certificate error");
                                                return S_OK;
                                            }).Get(),
                                        &tok);
                                }
#endif
                                EventRegistrationToken msgTok{};
                                m_impl->webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            if (!m_impl || !args) return S_OK;
                                            LPWSTR msg = nullptr;
                                            if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)) && msg) {
                                                m_impl->EmitWebMessage(msg);
                                                CoTaskMemFree(msg);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &msgTok);

                                m_impl->status = L"ready";
                                m_impl->Log(L"WebView2: controller ready");
                                if (!m_impl->pendingUrl.empty()) {
                                    m_impl->webview->Navigate(m_impl->pendingUrl.c_str());
                                    m_impl->pendingUrl.clear();
                                }
                            }
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        if (m_impl) m_impl->status = L"runtime-unavailable";
        if (m_impl) m_impl->Log(L"WebView2: CreateEnvironmentWithOptions call failed");
        return false;
    }
    return true;
#endif
}

void WebViewHost::Resize(const RECT& bounds) {
    if (!m_impl) return;
    m_impl->bounds = bounds;

#if LAN_HAS_WEBVIEW2
    if (m_impl->controller) {
        m_impl->controller->put_Bounds(bounds);
    }
#endif
}

void WebViewHost::Navigate(const std::wstring& url) {
    if (!m_impl) return;

#if LAN_HAS_WEBVIEW2
    if (m_impl->webview) {
        m_impl->webview->Navigate(url.c_str());
    } else {
        m_impl->pendingUrl = url;
    }
#else
    m_impl->pendingUrl = url;
    m_impl->Log(L"WebView2 SDK is unavailable; navigation request was stored but not displayed.");
#endif
}

void WebViewHost::SetMessageCallback(std::function<void(std::wstring)> webMessage) {
    if (!m_impl) return;
    m_impl->webMessage = std::move(webMessage);
}

bool WebViewHost::IsReady() const noexcept {
#if LAN_HAS_WEBVIEW2
    return m_impl && m_impl->webview != nullptr;
#else
    return false;
#endif
}

bool WebViewHost::IsAvailable() const noexcept {
    if (!m_impl) return false;
    return m_impl->status == L"initializing" || m_impl->status == L"ready";
}

std::wstring WebViewHost::StatusText() const {
    if (!m_impl) return L"not-initialized";
    return m_impl->status;
}
