#pragma once

#include "platform/abstraction/system_actions.h"

namespace lan::platform::posix {

class PosixSystemActions final : public ISystemActions {
public:
  const char* ProviderName() const override;
  bool OpenSystemPage(SystemPage page, std::string& err) override;
  bool OpenExternalUrl(std::string_view url, std::string& err) override;
  bool OpenExternalPath(const std::filesystem::path& path, std::string& err) override;
  bool ShowNotification(std::string_view title, std::string_view body, std::string& err) override;
};

} // namespace lan::platform::posix
