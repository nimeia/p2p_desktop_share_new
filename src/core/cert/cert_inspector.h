#pragma once

#include "core/cert/cert_types.h"

#include <string>

namespace lan::cert {

class CertInspector {
public:
  static bool InspectCertificate(const std::string& certFile,
                                 const std::string& keyFile,
                                 const std::string& expectedAltNames,
                                 CertStatus& status,
                                 std::string& err);
};

} // namespace lan::cert
