#include "core/cert/cert_manager.h"

#include "core/cert/cert_inspector.h"

namespace lan::cert {

bool CertManager::InspectCertificate(const std::string& certFile,
                                     const std::string& keyFile,
                                     const std::string& expectedAltNames,
                                     CertStatus& status,
                                     std::string& err) {
  return CertInspector::InspectCertificate(certFile, keyFile, expectedAltNames, status, err);
}

} // namespace lan::cert
