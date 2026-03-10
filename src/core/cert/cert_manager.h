#pragma once
#include <string>

namespace lan::cert {

struct CertPaths {
  std::string certFile;
  std::string keyFile;
};

class CertManager {
public:
  // Ensure certificate exists; if not, generate (M0: self-signed).
  // TODO: upgrade to local CA mode in M1.
  static bool EnsureSelfSigned(const std::string& outDir, const std::string& sanIp, CertPaths& paths, std::string& err);
};

} // namespace lan::cert
