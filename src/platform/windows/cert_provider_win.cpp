#include "platform/windows/cert_provider_win.h"

#include "platform/common/self_signed_cert_generator.h"


namespace lan::platform::windows {

const char* WindowsCertProvider::ProviderName() const {
  return "windows-cert-provider";
}

bool WindowsCertProvider::EnsureServerCertificate(const ServerCertificateRequest& request,
                                                  lan::cert::CertPaths& paths,
                                                  std::string& err) {
  return lan::platform::common::EnsureSelfSignedServerCertificate(request, paths, err);
}

} // namespace lan::platform::windows
