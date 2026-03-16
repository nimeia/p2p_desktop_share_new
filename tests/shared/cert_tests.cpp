#include "core/cert/cert_inspector.h"
#include "core/cert/cert_san.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    return false;
  }
  return true;
}

bool Contains(const std::vector<std::string>& values, const std::string& expected) {
  for (const auto& value : values) {
    if (value == expected) return true;
  }
  return false;
}

} // namespace

int main() {
  bool ok = true;

  const std::vector<std::string> parsed = lan::cert::SplitSanEntries(" 127.0.0.1 localhost LOCALHOST Example.COM.  example.com ");
  ok &= Expect(parsed.size() == 3, "SplitSanEntries should normalize and de-duplicate SAN entries.");
  ok &= Expect(Contains(parsed, "127.0.0.1"), "SplitSanEntries should preserve IPv4 SAN entries.");
  ok &= Expect(Contains(parsed, "localhost"), "SplitSanEntries should normalize localhost.");
  ok &= Expect(Contains(parsed, "example.com"), "SplitSanEntries should normalize DNS SAN entries.");

  const std::vector<std::string> expanded = lan::cert::ExpandServerCertificateAltNames("10.0.0.5");
  ok &= Expect(expanded.size() == 3, "ExpandServerCertificateAltNames should append local defaults.");
  ok &= Expect(Contains(expanded, "10.0.0.5"), "Expanded SAN list should retain requested host.");
  ok &= Expect(Contains(expanded, "127.0.0.1"), "Expanded SAN list should include 127.0.0.1.");
  ok &= Expect(Contains(expanded, "localhost"), "Expanded SAN list should include localhost.");

  lan::cert::CertStatus missing{};
  std::string err;
  const std::filesystem::path base = std::filesystem::temp_directory_path() / "lan_screenshare_cert_tests";
  const std::filesystem::path certFile = base / "missing.crt";
  const std::filesystem::path keyFile = base / "missing.key";
  const bool inspected = lan::cert::CertInspector::InspectCertificate(certFile.string(), keyFile.string(), "127.0.0.1", missing, err);
  ok &= Expect(inspected, "InspectCertificate should treat missing files as a valid inspection result.");
  ok &= Expect(!missing.ready, "Missing files should not produce a ready certificate state.");
  ok &= Expect(!missing.certExists && !missing.keyExists, "Missing files should be reflected in CertStatus.");
  ok &= Expect(missing.detail.find("missing") != std::string::npos, "Missing files should produce a readable detail message.");
  ok &= Expect(err.empty(), "Missing-file inspection should not set an error string.");

  if (!ok) return 1;
  std::cout << "cert tests passed\n";
  return 0;
}
