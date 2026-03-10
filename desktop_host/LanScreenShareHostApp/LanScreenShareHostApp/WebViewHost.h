#pragma once

#include <functional>
#include <string>
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
    void SetMessageCallback(std::function<void(std::wstring)> webMessage);
    bool IsReady() const noexcept;
    bool IsAvailable() const noexcept;
    std::wstring StatusText() const;

private:
    struct Impl;
    Impl* m_impl = nullptr;
};
