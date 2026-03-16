#pragma once

#include "platform/abstraction/cert_provider.h"

namespace lan::platform::windows {

class WindowsCertProvider final : public ICertProvider {
public:
  const char* ProviderName() const override;
  bool EnsureServerCertificate(const ServerCertificateRequest& request,
                               lan::cert::CertPaths& paths,
                               std::string& err) override;
};

} // namespace lan::platform::windows
