#include "platform/posix/cert_provider_posix.h"

#include "platform/common/self_signed_cert_generator.h"


namespace lan::platform::posix {

const char* PosixCertProvider::ProviderName() const {
  return "posix-cert-provider";
}

bool PosixCertProvider::EnsureServerCertificate(const ServerCertificateRequest& request,
                                                lan::cert::CertPaths& paths,
                                                std::string& err) {
  return lan::platform::common::EnsureSelfSignedServerCertificate(request, paths, err);
}

} // namespace lan::platform::posix
