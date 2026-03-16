#pragma once

#include <string>
#include <vector>

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

} // namespace lan::cert
