#include "pch.h"
#include "WebViewHost.h"

#include "WebViewRuntimeDetector.h"
#include "core/runtime/bootstrap_policy.h"

#include <fstream>
#include <wrl.h>
#include <wrl/event.h>
#include <sstream>

#if __has_include(<WebView2.h>)
#include <WebView2.h>
#define LAN_HAS_WEBVIEW2 1
#else
#define LAN_HAS_WEBVIEW2 0
#endif

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace fs = std::filesystem;

namespace {

std::wstring FormatHresult(HRESULT hr) {
    std::wstringstream ss;
    ss << L"0x"
       << std::uppercase
       << std::hex
       << std::setw(8)
       << std::setfill(L'0')
       << static_cast<unsigned long>(hr);
    return ss.str();
}

std::string Utf8FromWide(std::wstring_view text) {
    if (text.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring ResolveUserDataFolder() {
    fs::path base;
    wchar_t* localAppData = nullptr;
    size_t localAppDataLen = 0;
    if (_wdupenv_s(&localAppData, &localAppDataLen, L"LOCALAPPDATA") == 0 && localAppData && localAppData[0] != L'\0') {
        base = fs::path(localAppData);
        free(localAppData);
    } else {
        free(localAppData);
        std::error_code ec;
        base = fs::temp_directory_path(ec);
        if (ec) {
            base = fs::current_path();
        }
    }

    const fs::path dir = base / L"LanScreenShareHostApp" / L"WebView2" / std::to_wstring(GetCurrentProcessId());
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        return L"";
    }
    return dir.wstring();
}

std::wstring ResolveWebViewAdditionalBrowserArguments() {
    constexpr wchar_t kRequiredArgs[] = L"--disable-gpu --disable-gpu-compositing";

    wchar_t* existing = nullptr;
    std::size_t existingLen = 0;
    if (_wdupenv_s(&existing, &existingLen, L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS") != 0 || !existing || existing[0] == L'\0') {
        free(existing);
        return kRequiredArgs;
    }

    std::wstring combined = existing;
    free(existing);
    if (combined.find(L"--disable-gpu") != std::wstring::npos) {
        return combined;
    }
    if (!combined.empty() && combined.back() != L' ') {
        combined.push_back(L' ');
    }
    combined += kRequiredArgs;
    return combined;
}

}

struct WebViewHost::Impl {
    HWND parent = nullptr;
    RECT bounds{};
    std::function<void(std::wstring)> log;
    std::function<void(std::wstring)> webMessage;
    std::wstring pendingUrl;
    std::wstring currentUrl;
    std::wstring status = L"not-initialized";
    std::wstring detail;
    std::wstring userDataFolder;

#if LAN_HAS_WEBVIEW2
    ComPtr<ICoreWebView2Environment> env;
    ComPtr<ICoreWebView2Controller> controller;
    ComPtr<ICoreWebView2> webview;
#endif

    void Log(const std::wstring& msg) {
        if (!userDataFolder.empty()) {
            std::ofstream stream(fs::path(userDataFolder) / L"webview_trace.log", std::ios::app | std::ios::binary);
            if (stream.is_open()) {
                const auto utf8 = Utf8FromWide(msg);
                stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
                stream.write("\r\n", 2);
            }
        }
        if (log) log(msg);
    }

    void EmitWebMessage(const std::wstring& msg) {
        Log(L"WebView2: recv web message " + msg);
        if (webMessage) webMessage(msg);
    }

    void SetFailure(const std::wstring& nextStatus, HRESULT hr, const std::wstring& message) {
        status = nextStatus;
        detail = message + L" (" + FormatHresult(hr) + L")";
        Log(L"WebView2: " + detail);
    }
};

WebViewHost::~WebViewHost() {
    Reset();
}

void WebViewHost::Reset() noexcept {
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
    if (m_impl) {
        if (m_impl->status == L"initializing" || m_impl->status == L"ready") {
            return true;
        }
        Reset();
    }
    m_impl = new (std::nothrow) Impl();
    if (!m_impl) return false;

    m_impl->parent = parent;
    m_impl->bounds = bounds;
    m_impl->log = std::move(log);
    m_impl->webMessage = std::move(webMessage);
    m_impl->status = L"initializing";
    m_impl->detail.clear();
    m_impl->userDataFolder = ResolveUserDataFolder();
    Impl* current = m_impl;

#if !LAN_HAS_WEBVIEW2
    m_impl->status = L"sdk-unavailable";
    m_impl->detail = L"WebView2 SDK header not found at build time";
    m_impl->Log(L"WebView2 SDK header not found at build time; embedded host view is disabled.");
    return false;
#else
    const auto runtimeProbe = DetectWebView2Runtime();
    if (!runtimeProbe.installed) {
        m_impl->status = L"runtime-unavailable";
        m_impl->detail = runtimeProbe.detail;
        m_impl->Log(L"WebView2: " + runtimeProbe.detail);
        return false;
    }

    if (!m_impl->userDataFolder.empty()) {
        m_impl->Log(L"WebView2: user data folder " + m_impl->userDataFolder);
    } else {
        m_impl->Log(L"WebView2: user data folder fallback to default runtime location");
    }
    const std::wstring browserArgs = ResolveWebViewAdditionalBrowserArguments();
    SetEnvironmentVariableW(L"WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS", browserArgs.c_str());
    m_impl->Log(L"WebView2: browser arguments " + browserArgs);
    if (!runtimeProbe.version.empty()) {
        m_impl->Log(L"WebView2: detected runtime version " + runtimeProbe.version);
    }

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr,
        m_impl->userDataFolder.empty() ? nullptr : m_impl->userDataFolder.c_str(),
        nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, current](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (!m_impl || m_impl != current) return E_FAIL;
                if (FAILED(result) || !env) {
                    current->SetFailure(L"runtime-unavailable",
                                        FAILED(result) ? result : E_FAIL,
                                        L"CreateEnvironment failed");
                    return FAILED(result) ? result : E_FAIL;
                }

                current->env = env;
                current->detail.clear();
                current->Log(L"WebView2: environment created");

                return current->env->CreateCoreWebView2Controller(
                    current->parent,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, current](HRESULT result2, ICoreWebView2Controller* controller) -> HRESULT {
                            if (!m_impl || m_impl != current) return E_FAIL;
                            if (FAILED(result2) || !controller) {
                                current->SetFailure(L"controller-unavailable",
                                                    FAILED(result2) ? result2 : E_FAIL,
                                                    L"CreateController failed");
                                return FAILED(result2) ? result2 : E_FAIL;
                            }

                            current->controller = controller;
                            current->controller->put_Bounds(current->bounds);

                            ComPtr<ICoreWebView2> wv;
                            if (SUCCEEDED(current->controller->get_CoreWebView2(&wv)) && wv) {
                                current->webview = wv;
                            }

                            if (current->webview) {
#if defined(__ICoreWebView2_14_INTERFACE_DEFINED__)
                                ComPtr<ICoreWebView2_14> wv14;
                                if (SUCCEEDED(current->webview.As(&wv14)) && wv14) {
                                    EventRegistrationToken tok{};
                                    wv14->add_ServerCertificateErrorDetected(
                                        Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
                                            [this](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* args) -> HRESULT {
                                                if (!args) return S_OK;
                                                if (!m_impl) return S_OK;
                                                const std::wstring activeUrl = !m_impl->currentUrl.empty() ? m_impl->currentUrl : m_impl->pendingUrl;
                                                if (lan::runtime::ShouldBypassLocalCertificateForUrl(activeUrl)) {
                                                    args->put_Action(COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_ALWAYS_ALLOW);
                                                    m_impl->Log(L"WebView2: allowed local certificate error for " + activeUrl);
                                                } else {
                                                    args->put_Action(COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_CANCEL);
                                                    m_impl->Log(L"WebView2: blocked certificate bypass for non-local URL " + activeUrl);
                                                }
                                                return S_OK;
                                            }).Get(),
                                        &tok);
                                }
#endif
                                EventRegistrationToken msgTok{};
                                current->webview->add_WebMessageReceived(
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

                                EventRegistrationToken navStartTok{};
                                current->webview->add_NavigationStarting(
                                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                            if (!m_impl || !args) return S_OK;
                                            LPWSTR uri = nullptr;
                                            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                                m_impl->Log(L"WebView2: navigating to " + std::wstring(uri));
                                                CoTaskMemFree(uri);
                                            }
                                            m_impl->detail.clear();
                                            return S_OK;
                                        }).Get(),
                                    &navStartTok);

                                EventRegistrationToken navCompletedTok{};
                                current->webview->add_NavigationCompleted(
                                    Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                        [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                                            if (!m_impl || !args) return S_OK;

                                            BOOL success = FALSE;
                                            if (SUCCEEDED(args->get_IsSuccess(&success)) && success) {
                                                m_impl->detail.clear();
                                                m_impl->Log(L"WebView2: navigation completed");
                                                const std::wstring activeUrl = !m_impl->currentUrl.empty() ? m_impl->currentUrl : m_impl->pendingUrl;
                                                if (m_impl->webview && activeUrl.find(L"/admin/") != std::wstring::npos) {
                                                    static constexpr wchar_t kAdminDomProbe[] = LR"JS(
(() => {
  try {
    const shell = document.querySelector('.shell');
    const activeRoute = Array.from(document.querySelectorAll('[data-route-view]')).find((node) => !node.hasAttribute('hidden'));
    const shellStyle = shell ? getComputedStyle(shell) : null;
    const bodyStyle = document.body ? getComputedStyle(document.body) : null;
    const rect = shell ? shell.getBoundingClientRect() : { width: 0, height: 0 };
    const text = shell ? shell.innerText.trim() : (document.body ? document.body.innerText.trim() : '');
    return {
      title: document.title || '',
      readyState: document.readyState,
      shellPresent: !!shell,
      activeRoute: activeRoute ? activeRoute.getAttribute('data-route-view') : '',
      shellDisplay: shellStyle ? shellStyle.display : '',
      shellVisibility: shellStyle ? shellStyle.visibility : '',
      shellOpacity: shellStyle ? shellStyle.opacity : '',
      shellWidth: Math.round(rect.width || 0),
      shellHeight: Math.round(rect.height || 0),
      bodyBackgroundColor: bodyStyle ? bodyStyle.backgroundColor : '',
      textLength: text.length,
      textSample: text.slice(0, 180)
    };
  } catch (error) {
    return { probeError: String(error) };
  }
})()
)JS";
                                                    m_impl->webview->ExecuteScript(
                                                        kAdminDomProbe,
                                                        Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                                                            [this](HRESULT scriptResult, LPCWSTR resultObjectAsJson) -> HRESULT {
                                                                if (!m_impl) return S_OK;
                                                                if (FAILED(scriptResult)) {
                                                                    m_impl->Log(L"WebView2: admin DOM probe failed " + FormatHresult(scriptResult));
                                                                    return S_OK;
                                                                }
                                                                m_impl->Log(L"WebView2: admin DOM probe " +
                                                                            std::wstring(resultObjectAsJson ? resultObjectAsJson : L"null"));
                                                                return S_OK;
                                                            }).Get());
                                                }
                                                return S_OK;
                                            }

                                            COREWEBVIEW2_WEB_ERROR_STATUS errorStatus = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
                                            args->get_WebErrorStatus(&errorStatus);
                                            m_impl->detail = L"Navigation failed with WebErrorStatus=" + std::to_wstring(static_cast<int>(errorStatus));
                                            m_impl->Log(L"WebView2: " + m_impl->detail);
                                            return S_OK;
                                        }).Get(),
                                    &navCompletedTok);

                                current->status = L"ready";
                                current->detail.clear();
                                current->Log(L"WebView2: controller ready");
                                if (!current->pendingUrl.empty()) {
                                    current->currentUrl = current->pendingUrl;
                                    current->webview->Navigate(current->pendingUrl.c_str());
                                    current->pendingUrl.clear();
                                }
                            }
                            return S_OK;
                        }).Get());
            }).Get());

    if (FAILED(hr)) {
        if (m_impl) {
            m_impl->SetFailure(L"runtime-unavailable", hr, L"CreateEnvironmentWithOptions call failed");
        }
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
        m_impl->currentUrl = url;
        m_impl->webview->Navigate(url.c_str());
    } else {
        m_impl->pendingUrl = url;
        m_impl->currentUrl = url;
    }
#else
    m_impl->pendingUrl = url;
    m_impl->Log(L"WebView2 SDK is unavailable; navigation request was stored but not displayed.");
#endif
}

bool WebViewHost::PostJson(std::wstring_view json) {
    if (!m_impl) return false;

#if LAN_HAS_WEBVIEW2
    if (!m_impl->webview) {
        return false;
    }

    const HRESULT hr = m_impl->webview->PostWebMessageAsJson(std::wstring(json).c_str());
    if (FAILED(hr)) {
        m_impl->Log(L"WebView2: PostWebMessageAsJson failed");
        return false;
    }
    return true;
#else
    (void)json;
    m_impl->Log(L"WebView2 SDK is unavailable; PostJson was ignored.");
    return false;
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

std::wstring WebViewHost::DetailText() const {
    if (!m_impl) return L"";
    return m_impl->detail;
}
