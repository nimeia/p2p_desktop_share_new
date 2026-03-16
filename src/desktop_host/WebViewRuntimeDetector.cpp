#include "pch.h"
#include "WebViewRuntimeDetector.h"

#include <windows.h>

namespace {

constexpr wchar_t kEvergreenClientGuid[] = L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

std::wstring QueryVersionValue(HKEY hive, const wchar_t* subKey) {
    wchar_t buffer[256]{};
    DWORD type = 0;
    DWORD size = sizeof(buffer);
    const LONG rc = RegGetValueW(hive,
                                 subKey,
                                 L"pv",
                                 RRF_RT_REG_SZ,
                                 &type,
                                 buffer,
                                 &size);
    if (rc != ERROR_SUCCESS) {
        return {};
    }
    std::wstring value(buffer);
    return value;
}

bool IsUsableVersion(const std::wstring& version) {
    return !version.empty() && version != L"0.0.0.0";
}

std::wstring MachineRuntimeSubKey() {
#if defined(_WIN64)
    return std::wstring(L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\") + kEvergreenClientGuid;
#else
    return std::wstring(L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\") + kEvergreenClientGuid;
#endif
}

std::wstring UserRuntimeSubKey() {
    return std::wstring(L"Software\\Microsoft\\EdgeUpdate\\Clients\\") + kEvergreenClientGuid;
}

} // namespace

WebViewRuntimeProbe DetectWebView2Runtime() {
    WebViewRuntimeProbe probe;

    const std::wstring machineVersion = QueryVersionValue(HKEY_LOCAL_MACHINE, MachineRuntimeSubKey().c_str());
    const std::wstring userVersion = QueryVersionValue(HKEY_CURRENT_USER, UserRuntimeSubKey().c_str());

    probe.machineInstall = IsUsableVersion(machineVersion);
    probe.userInstall = IsUsableVersion(userVersion);
    probe.installed = probe.machineInstall || probe.userInstall;
    probe.version = probe.machineInstall ? machineVersion : userVersion;

    if (probe.installed) {
        if (probe.machineInstall && probe.userInstall) {
            probe.detail = L"Evergreen WebView2 Runtime detected for both machine and current user.";
        } else if (probe.machineInstall) {
            probe.detail = L"Evergreen WebView2 Runtime detected from the machine-wide install.";
        } else {
            probe.detail = L"Evergreen WebView2 Runtime detected from the current-user install.";
        }
        if (!probe.version.empty()) {
            probe.detail += L" Version: " + probe.version + L".";
        }
        return probe;
    }

    probe.detail = L"Evergreen WebView2 Runtime was not detected in the standard EdgeUpdate registry locations. Install or repair the runtime, then relaunch the desktop host.";
    return probe;
}
