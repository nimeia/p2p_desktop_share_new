#import <Cocoa/Cocoa.h>

#include "platform/macos/system_actions_macos.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace lan::platform::macos {
namespace {

NSString* ToNSString(std::string_view value) {
  return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
}

bool OpenUrlString(std::string_view url, std::string& err) {
  if (url.empty()) {
    err = "URL is empty.";
    return false;
  }

  NSString* urlString = ToNSString(url);
  NSURL* target = [NSURL URLWithString:urlString];
  if (!target) {
    err = "URL is invalid.";
    return false;
  }
  if (![[NSWorkspace sharedWorkspace] openURL:target]) {
    err = "NSWorkspace failed to open the requested URL.";
    return false;
  }
  err.clear();
  return true;
}

bool OpenFilePath(const std::filesystem::path& path, std::string& err) {
  if (path.empty()) {
    err = "Path is empty.";
    return false;
  }
  NSString* value = ToNSString(path.string());
  if (!value) {
    err = "Path is not valid UTF-8.";
    return false;
  }
  NSURL* target = [NSURL fileURLWithPath:value];
  if (!target) {
    err = "Path could not be converted to a file URL.";
    return false;
  }
  if (![[NSWorkspace sharedWorkspace] openURL:target]) {
    err = "NSWorkspace failed to open the requested path.";
    return false;
  }
  err.clear();
  return true;
}

} // namespace

const char* MacOSSystemActions::ProviderName() const {
  return "macos-cocoa";
}

bool MacOSSystemActions::OpenSystemPage(SystemPage page, std::string& err) {
  switch (page) {
    case SystemPage::ConnectedDevices:
      return OpenUrlString("x-apple.systempreferences:com.apple.BluetoothSettings", err);
    case SystemPage::MobileHotspot:
      return OpenUrlString("x-apple.systempreferences:com.apple.NetworkSettings", err);
    default:
      err = "Unknown system page.";
      return false;
  }
}

bool MacOSSystemActions::OpenExternalUrl(std::string_view url, std::string& err) {
  return OpenUrlString(url, err);
}

bool MacOSSystemActions::OpenExternalPath(const std::filesystem::path& path, std::string& err) {
  return OpenFilePath(path, err);
}

bool MacOSSystemActions::ShowNotification(std::string_view title, std::string_view body, std::string& err) {
  NSString* titleText = ToNSString(title);
  NSString* bodyText = ToNSString(body);
  if (!titleText || !bodyText) {
    err = "Notification text is not valid UTF-8.";
    return false;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSUserNotification* notification = [[NSUserNotification alloc] init];
  notification.title = titleText;
  notification.informativeText = bodyText;
  [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
#pragma clang diagnostic pop

  err.clear();
  return true;
}

} // namespace lan::platform::macos
