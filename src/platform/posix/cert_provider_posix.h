#pragma once

#include "platform/abstraction/cert_provider.h"

namespace lan::platform::posix {

class PosixCertProvider final : public ICertProvider {
public:
  const char* ProviderName() const override;
  bool EnsureServerCertificate(const ServerCertificateRequest& request,
                               lan::cert::CertPaths& paths,
                               std::string& err) override;
};

} // namespace lan::platform::posix
