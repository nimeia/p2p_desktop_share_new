#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <windows.h>

class WebViewHost {
public:
    WebViewHost() = default;
    ~WebViewHost();

    WebViewHost(const WebViewHost&) = delete;
    WebViewHost& operator=(const WebViewHost&) = delete;

    bool Initialize(HWND parent, const RECT& bounds, std::function<void(std::wstring)> log, std::function<void(std::wstring)> webMessage = {});
    void Resize(const RECT& bounds);
    void Navigate(const std::wstring& url);
    bool PostJson(std::wstring_view json);
    void SetMessageCallback(std::function<void(std::wstring)> webMessage);
    void Reset() noexcept;
    bool IsReady() const noexcept;
    bool IsAvailable() const noexcept;
    std::wstring StatusText() const;
    std::wstring DetailText() const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
