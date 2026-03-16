#pragma once

#include "core/cert/cert_types.h"

#include <string>

namespace lan::platform {

struct ServerCertificateRequest {
  std::string outputDirectory;
  std::string subjectAltNames;
};

class ICertProvider {
public:
  virtual ~ICertProvider() = default;

  virtual const char* ProviderName() const = 0;
  virtual bool EnsureServerCertificate(const ServerCertificateRequest& request,
                                       lan::cert::CertPaths& paths,
                                       std::string& err) = 0;
};

} // namespace lan::platform
