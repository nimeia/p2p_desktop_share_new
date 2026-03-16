#pragma once

#include <string>

struct WebViewRuntimeProbe {
    bool installed = false;
    bool machineInstall = false;
    bool userInstall = false;
    std::wstring version;
    std::wstring detail;
};

WebViewRuntimeProbe DetectWebView2Runtime();
