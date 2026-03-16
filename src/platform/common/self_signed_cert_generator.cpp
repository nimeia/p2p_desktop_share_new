#include "platform/common/self_signed_cert_generator.h"

#include "core/cert/cert_inspector.h"
#include "core/cert/cert_san.h"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <openssl/asn1.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace lan::platform::common {
namespace {

namespace fs = std::filesystem;

struct BioDeleter {
  void operator()(BIO* bio) const {
    if (bio) BIO_free(bio);
  }
};

struct PKeyDeleter {
  void operator()(EVP_PKEY* key) const {
    if (key) EVP_PKEY_free(key);
  }
};

struct PKeyCtxDeleter {
  void operator()(EVP_PKEY_CTX* ctx) const {
    if (ctx) EVP_PKEY_CTX_free(ctx);
  }
};

struct X509Deleter {
  void operator()(X509* cert) const {
    if (cert) X509_free(cert);
  }
};

using UniqueBio = std::unique_ptr<BIO, BioDeleter>;
using UniquePKey = std::unique_ptr<EVP_PKEY, PKeyDeleter>;
using UniquePKeyCtx = std::unique_ptr<EVP_PKEY_CTX, PKeyCtxDeleter>;
using UniqueX509 = std::unique_ptr<X509, X509Deleter>;

bool WriteFile(const std::string& path, const std::string& content, std::string& err) {
  std::ofstream output(fs::path(path), std::ios::binary | std::ios::trunc);
  if (!output) {
    err = "failed to open output file: " + path;
    return false;
  }
  output.write(content.data(), static_cast<std::streamsize>(content.size()));
  if (!output.good()) {
    err = "failed to write output file: " + path;
    return false;
  }
  return true;
}

std::string DrainBio(BIO* bio) {
  BUF_MEM* buffer = nullptr;
  BIO_get_mem_ptr(bio, &buffer);
  if (!buffer || !buffer->data || buffer->length == 0) return {};
  return std::string(buffer->data, buffer->length);
}

std::string ChooseCommonName(const std::vector<std::string>& sans) {
  for (const auto& entry : sans) {
    if (!lan::cert::LooksLikeIpAddress(entry)) return entry;
  }
  for (const auto& entry : sans) {
    if (lan::cert::LooksLikeIpv4(entry)) return entry;
  }
  return sans.empty() ? "LAN Screen Share" : sans.front();
}

std::string BuildSubjectAltNameValue(const std::vector<std::string>& sans) {
  std::string value;
  for (std::size_t i = 0; i < sans.size(); ++i) {
    if (i != 0) value += ',';
    value += lan::cert::LooksLikeIpAddress(sans[i]) ? "IP:" : "DNS:";
    value += sans[i];
  }
  return value;
}

bool AddX509Extension(X509* cert,
                      X509V3_CTX* ctx,
                      int nid,
                      const std::string& value,
                      std::string& err) {
  X509_EXTENSION* extension = X509V3_EXT_conf_nid(nullptr, ctx, nid, const_cast<char*>(value.c_str()));
  if (!extension) {
    err = "OpenSSL failed to create X509 extension nid=" + std::to_string(nid);
    return false;
  }
  const int added = X509_add_ext(cert, extension, -1);
  X509_EXTENSION_free(extension);
  if (added != 1) {
    err = "OpenSSL failed to append X509 extension nid=" + std::to_string(nid);
    return false;
  }
  return true;
}

bool GenerateSelfSignedWithOpenSsl(const std::string& keyFile,
                                   const std::string& certFile,
                                   const std::vector<std::string>& sans,
                                   std::string& err) {
  UniquePKeyCtx keyContext(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr));
  if (!keyContext) {
    err = "OpenSSL failed to allocate EVP_PKEY context.";
    return false;
  }

  EVP_PKEY* rawKey = nullptr;
  if (EVP_PKEY_keygen_init(keyContext.get()) <= 0 ||
      EVP_PKEY_CTX_set_rsa_keygen_bits(keyContext.get(), 2048) <= 0 ||
      EVP_PKEY_keygen(keyContext.get(), &rawKey) <= 0) {
    err = "OpenSSL failed to generate RSA private key.";
    return false;
  }
  UniquePKey key(rawKey);

  UniqueX509 cert(X509_new());
  if (!cert) {
    err = "OpenSSL failed to allocate X509 certificate.";
    return false;
  }

  if (X509_set_version(cert.get(), 2) != 1) {
    err = "OpenSSL failed to set X509 version.";
    return false;
  }
  ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), static_cast<long>(std::time(nullptr)));
  if (X509_gmtime_adj(X509_getm_notBefore(cert.get()), 0) == nullptr ||
      X509_gmtime_adj(X509_getm_notAfter(cert.get()), 60L * 60L * 24L * 3650L) == nullptr) {
    err = "OpenSSL failed to set certificate validity window.";
    return false;
  }

  if (X509_set_pubkey(cert.get(), key.get()) != 1) {
    err = "OpenSSL failed to attach public key to certificate.";
    return false;
  }

  X509_NAME* subject = X509_get_subject_name(cert.get());
  const std::string commonName = ChooseCommonName(sans);
  if (!subject || X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
                                             reinterpret_cast<const unsigned char*>(commonName.c_str()),
                                             -1, -1, 0) != 1) {
    err = "OpenSSL failed to set certificate common name.";
    return false;
  }
  if (X509_set_issuer_name(cert.get(), subject) != 1) {
    err = "OpenSSL failed to set certificate issuer.";
    return false;
  }

  X509V3_CTX extensionContext{};
  X509V3_set_ctx(&extensionContext, cert.get(), cert.get(), nullptr, nullptr, 0);
  if (!AddX509Extension(cert.get(), &extensionContext, NID_basic_constraints, "CA:FALSE", err) ||
      !AddX509Extension(cert.get(), &extensionContext, NID_key_usage, "digitalSignature,keyEncipherment", err) ||
      !AddX509Extension(cert.get(), &extensionContext, NID_ext_key_usage, "serverAuth", err) ||
      !AddX509Extension(cert.get(), &extensionContext, NID_subject_alt_name, BuildSubjectAltNameValue(sans), err)) {
    return false;
  }

  if (X509_sign(cert.get(), key.get(), EVP_sha256()) <= 0) {
    err = "OpenSSL failed to sign certificate.";
    return false;
  }

  UniqueBio keyBio(BIO_new(BIO_s_mem()));
  UniqueBio certBio(BIO_new(BIO_s_mem()));
  if (!keyBio || !certBio) {
    err = "OpenSSL failed to allocate memory BIO.";
    return false;
  }

  if (PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) != 1) {
    err = "OpenSSL failed to write PEM private key.";
    return false;
  }
  if (PEM_write_bio_X509(certBio.get(), cert.get()) != 1) {
    err = "OpenSSL failed to write PEM certificate.";
    return false;
  }

  const std::string keyPem = DrainBio(keyBio.get());
  const std::string certPem = DrainBio(certBio.get());
  if (keyPem.empty() || certPem.empty()) {
    err = "OpenSSL produced empty PEM output.";
    return false;
  }

  if (!WriteFile(keyFile, keyPem, err) || !WriteFile(certFile, certPem, err)) {
    return false;
  }

  err.clear();
  return true;
}

} // namespace

bool EnsureSelfSignedServerCertificate(const ServerCertificateRequest& request,
                                       lan::cert::CertPaths& paths,
                                       std::string& err) {
  fs::create_directories(fs::path(request.outputDirectory));

  paths.keyFile = (fs::path(request.outputDirectory) / "server.key").string();
  paths.certFile = (fs::path(request.outputDirectory) / "server.crt").string();

  lan::cert::CertStatus current{};
  std::string inspectErr;
  if (lan::cert::CertInspector::InspectCertificate(paths.certFile, paths.keyFile, request.subjectAltNames, current, inspectErr) && current.ready) {
    err.clear();
    return true;
  }

  const std::vector<std::string> effectiveSans = lan::cert::ExpandServerCertificateAltNames(request.subjectAltNames);

  std::error_code removeEc;
  fs::remove(fs::path(paths.keyFile), removeEc);
  removeEc.clear();
  fs::remove(fs::path(paths.certFile), removeEc);

  if (!GenerateSelfSignedWithOpenSsl(paths.keyFile, paths.certFile, effectiveSans, err)) {
    return false;
  }

  lan::cert::CertStatus finalStatus{};
  if (!lan::cert::CertInspector::InspectCertificate(paths.certFile, paths.keyFile, request.subjectAltNames, finalStatus, inspectErr)) {
    err = inspectErr.empty() ? "Certificate generation succeeded, but validation failed." : inspectErr;
    return false;
  }
  if (!finalStatus.ready) {
    err = finalStatus.detail.empty() ? "Generated certificate does not match the requested SANs." : finalStatus.detail;
    return false;
  }

  err.clear();
  return true;
}

} // namespace lan::platform::common
