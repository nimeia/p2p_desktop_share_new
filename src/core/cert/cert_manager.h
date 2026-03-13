#pragma once
#include <vector>
#include <string>

namespace lan::cert {

struct CertPaths {
  std::string certFile;
  std::string keyFile;
};

struct CertStatus {
  bool certExists = false;
  bool keyExists = false;
  bool certParsable = false;
  bool validNow = false;
  bool sanMatches = false;
  bool ready = false;
  std::string detail;
  std::vector<std::string> expectedAltNames;
  std::vector<std::string> missingAltNames;
  std::vector<std::string> presentIpSans;
  std::vector<std::string> presentDnsSans;
};

class CertManager {
public:
  // Ensure certificate exists; if not, generate (M0: self-signed).
  // TODO: upgrade to local CA mode in M1.
  static bool EnsureSelfSigned(const std::string& outDir, const std::string& sanIp, CertPaths& paths, std::string& err);
  static bool InspectCertificate(const std::string& certFile,
                                 const std::string& keyFile,
                                 const std::string& sanIp,
                                 CertStatus& status,
                                 std::string& err);
};

} // namespace lan::cert
