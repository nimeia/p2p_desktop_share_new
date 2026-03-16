#pragma once

#include "platform/abstraction/cert_provider.h"

namespace lan::platform::common {

bool EnsureSelfSignedServerCertificate(const ServerCertificateRequest& request,
                                       lan::cert::CertPaths& paths,
                                       std::string& err);

} // namespace lan::platform::common
