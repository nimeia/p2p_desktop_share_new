#include "core/cert/cert_inspector.h"

#include "core/cert/cert_san.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace lan::cert {
namespace {

namespace fs = std::filesystem;

struct BioDeleter {
  void operator()(BIO* bio) const {
    if (bio) BIO_free(bio);
  }
};

struct X509Deleter {
  void operator()(X509* cert) const {
    if (cert) X509_free(cert);
  }
};

using UniqueBio = std::unique_ptr<BIO, BioDeleter>;
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

bool ReadFileToString(const std::string& path, std::string& content) {
  std::ifstream input(fs::path(path), std::ios::binary);
  if (!input) return false;
  content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  return true;
}

UniqueX509 ReadCertificate(const std::string& certFile, std::string& err) {
  std::string pem;
  if (!ReadFileToString(certFile, pem)) {
    err = "Failed to read certificate file.";
    return {};
  }

  UniqueBio bio(BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size())));
  if (!bio) {
    err = "Failed to create OpenSSL BIO for certificate file.";
    return {};
  }

  X509* cert = PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr);
  if (!cert) {
    err = "Certificate file exists, but OpenSSL could not parse it.";
    return {};
  }

  err.clear();
  return UniqueX509(cert);
}

std::string RenderGeneralName(const GENERAL_NAME* name) {
  if (!name) return {};
  UniqueBio bio(BIO_new(BIO_s_mem()));
  if (!bio) return {};
  if (GENERAL_NAME_print(bio.get(), const_cast<GENERAL_NAME*>(name)) != 1) {
    return {};
  }

  BUF_MEM* buffer = nullptr;
  BIO_get_mem_ptr(bio.get(), &buffer);
  if (!buffer || !buffer->data || buffer->length == 0) return {};
  return std::string(buffer->data, buffer->length);
}

void PushUnique(std::vector<std::string>& values, std::string value) {
  if (value.empty()) return;
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(std::move(value));
  }
}

void CollectSubjectAltNames(X509* cert, CertStatus& status) {
  GENERAL_NAMES* names = static_cast<GENERAL_NAMES*>(X509_get_ext_d2i(cert, NID_subject_alt_name, nullptr, nullptr));
  if (!names) return;

  const int count = sk_GENERAL_NAME_num(names);
  for (int i = 0; i < count; ++i) {
    const GENERAL_NAME* name = sk_GENERAL_NAME_value(names, i);
    if (!name) continue;

    if (name->type == GEN_DNS && name->d.dNSName) {
      const unsigned char* data = ASN1_STRING_get0_data(name->d.dNSName);
      const int length = ASN1_STRING_length(name->d.dNSName);
      if (data && length > 0) {
        PushUnique(status.presentDnsSans, NormalizeDnsName(std::string(reinterpret_cast<const char*>(data), static_cast<std::size_t>(length))));
      }
      continue;
    }

    if (name->type == GEN_IPADD) {
      std::string rendered = RenderGeneralName(name);
      constexpr const char* prefix = "IP Address:";
      if (rendered.rfind(prefix, 0) == 0) {
        rendered = rendered.substr(std::char_traits<char>::length(prefix));
      }
      PushUnique(status.presentIpSans, NormalizeSanEntry(std::move(rendered)));
    }
  }

  GENERAL_NAMES_free(names);
}

void PopulateDetail(CertStatus& status) {
  if (!status.certParsable) {
    status.detail = "Certificate file is not parseable.";
  } else if (!status.validNow) {
    status.detail = "Certificate is expired or not yet valid.";
  } else if (!status.sanMatches) {
    status.detail = "Certificate SAN does not match the current host entries. Missing: " + JoinValues(status.missingAltNames);
  } else {
    status.detail = "Certificate matches the current host entries.";
  }
}

} // namespace

bool CertInspector::InspectCertificate(const std::string& certFile,
                                       const std::string& keyFile,
                                       const std::string& expectedAltNames,
                                       CertStatus& status,
                                       std::string& err) {
  status = {};
  status.certExists = fs::exists(fs::path(certFile));
  status.keyExists = fs::exists(fs::path(keyFile));
  status.expectedAltNames = SplitSanEntries(expectedAltNames);

  if (!status.certExists || !status.keyExists) {
    status.detail = !status.certExists && !status.keyExists
        ? "Certificate and key files are missing."
        : (!status.certExists ? "Certificate file is missing." : "Certificate key file is missing.");
    err.clear();
    return true;
  }

  std::string parseErr;
  UniqueX509 cert = ReadCertificate(certFile, parseErr);
  if (!cert) {
    status.detail = parseErr.empty() ? "Certificate file exists, but OpenSSL could not parse it." : parseErr;
    err = status.detail;
    return false;
  }

  status.certParsable = true;
  status.validNow =
      X509_cmp_current_time(X509_get0_notBefore(cert.get())) <= 0 &&
      X509_cmp_current_time(X509_get0_notAfter(cert.get())) >= 0;

  CollectSubjectAltNames(cert.get(), status);

  for (const auto& expected : status.expectedAltNames) {
    const bool isIp = LooksLikeIpAddress(expected);
    const auto& present = isIp ? status.presentIpSans : status.presentDnsSans;
    if (std::find(present.begin(), present.end(), expected) == present.end()) {
      status.missingAltNames.push_back(expected);
    }
  }

  status.sanMatches = status.missingAltNames.empty();
  status.ready = status.certParsable && status.validNow && status.sanMatches;
  PopulateDetail(status);
  err.clear();
  return true;
}

} // namespace lan::cert
