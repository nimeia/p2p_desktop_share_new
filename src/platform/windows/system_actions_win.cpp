#include "platform/windows/system_actions_win.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <windows.h>
#endif

#include <string>
#include <string_view>

namespace lan::platform::windows {
#if defined(_WIN32)
namespace {

std::wstring Utf8ToWide(std::string_view value) {
  if (value.empty()) return {};
  const int needed = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (needed <= 0) return {};
  std::wstring out(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), needed);
  return out;
}

bool ShellOpen(std::wstring_view target, std::string& err) {
  err.clear();
  const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", std::wstring(target).c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  if (result <= 32) {
    err = "ShellExecuteW failed.";
    return false;
  }
  return true;
}

} // namespace
#endif

const char* WindowsSystemActions::ProviderName() const {
  return "windows-shell";
}

bool WindowsSystemActions::OpenSystemPage(SystemPage page, std::string& err) {
#if defined(_WIN32)
  switch (page) {
  case SystemPage::ConnectedDevices:
    return ShellOpen(L"ms-settings:connecteddevices", err);
  case SystemPage::MobileHotspot:
    return ShellOpen(L"ms-settings:network-mobilehotspot", err);
  case SystemPage::Firewall:
    return ShellOpen(L"firewall.cpl", err);
  default:
    err = "Unknown system page.";
    return false;
  }
#else
  err = "Windows system actions are unavailable on this platform.";
  return false;
#endif
}

bool WindowsSystemActions::OpenExternalUrl(std::string_view url, std::string& err) {
#if defined(_WIN32)
  const auto wide = Utf8ToWide(url);
  if (wide.empty()) {
    err = "URL is empty or invalid UTF-8.";
    return false;
  }
  return ShellOpen(wide, err);
#else
  err = "Windows system actions are unavailable on this platform.";
  return false;
#endif
}

bool WindowsSystemActions::OpenExternalPath(const std::filesystem::path& path, std::string& err) {
#if defined(_WIN32)
  if (path.empty()) {
    err = "Path is empty.";
    return false;
  }
  return ShellOpen(path.native(), err);
#else
  err = "Windows system actions are unavailable on this platform.";
  return false;
#endif
}

bool WindowsSystemActions::ShowNotification(std::string_view title, std::string_view body, std::string& err) {
  (void)title;
  (void)body;
  err.clear();
#if defined(_WIN32)
  return true;
#else
  return true;
#endif
}

} // namespace lan::platform::windows
