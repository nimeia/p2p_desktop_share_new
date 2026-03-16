#include "platform/posix/system_actions_posix.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>

namespace lan::platform::posix {
namespace {

std::string EscapeShell(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (char ch : value) {
    if (ch == '\'' ) out += "'\\''";
    else out.push_back(ch);
  }
  return out;
}

std::string OpenCommand() {
#if defined(__APPLE__)
  return "open";
#else
  return "xdg-open";
#endif
}

bool LaunchOpen(std::string_view target, std::string& err) {
  if (target.empty()) {
    err = "Open target is empty.";
    return false;
  }
  const std::string command = OpenCommand() + " '" + EscapeShell(target) + "' >/dev/null 2>&1 &";
  const int rc = std::system(command.c_str());
  if (rc != 0) {
    err = "Failed to launch system open command.";
    return false;
  }
  err.clear();
  return true;
}

std::string PathToUtf8(const std::filesystem::path& path) {
#if defined(__cpp_lib_char8_t)
  const auto u8 = path.u8string();
  return std::string(reinterpret_cast<const char*>(u8.c_str()), u8.size());
#else
  return path.u8string();
#endif
}

} // namespace

const char* PosixSystemActions::ProviderName() const {
  return "posix-shell-open";
}

bool PosixSystemActions::OpenSystemPage(SystemPage page, std::string& err) {
  switch (page) {
  case SystemPage::ConnectedDevices:
    err = "Connected devices pairing does not have a portable POSIX settings entry in this MVP.";
    return false;
  case SystemPage::MobileHotspot:
    err = "Mobile hotspot settings are not implemented on POSIX in this MVP.";
    return false;
  default:
    err = "Unknown system page.";
    return false;
  }
}

bool PosixSystemActions::OpenExternalUrl(std::string_view url, std::string& err) {
  return LaunchOpen(url, err);
}

bool PosixSystemActions::OpenExternalPath(const std::filesystem::path& path, std::string& err) {
  return LaunchOpen(PathToUtf8(path), err);
}

bool PosixSystemActions::ShowNotification(std::string_view title, std::string_view body, std::string& err) {
#if defined(__APPLE__)
  const std::string command = "osascript -e 'display notification "" + EscapeShell(body) + "" with title "" + EscapeShell(title) + ""' >/dev/null 2>&1";
#else
  const std::string command = "notify-send '" + EscapeShell(title) + "' '" + EscapeShell(body) + "' >/dev/null 2>&1";
#endif
  const int rc = std::system(command.c_str());
  if (rc != 0) {
    err = "Failed to launch system notification command.";
    return false;
  }
  err.clear();
  return true;
}

} // namespace lan::platform::posix
