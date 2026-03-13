#include "cert_manager.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

#if defined(LAN_CERT_USE_OPENSSL_API)
  #include <openssl/evp.h>
  #include <openssl/pem.h>
  #include <openssl/rsa.h>
  #include <openssl/x509.h>
  #include <openssl/x509v3.h>
  #if defined(_WIN32)
    #include <openssl/applink.c>
  #endif
#endif

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <wincrypt.h>
  #pragma comment(lib, "ws2_32.lib")
  #pragma comment(lib, "crypt32.lib")
#endif

namespace lan::cert {

namespace fs = std::filesystem;

static std::vector<std::string> SplitIps(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&](){
    if (!cur.empty()) { out.push_back(cur); cur.clear(); }
  };
  for (char c : s) {
    if (c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
      flush();
    } else {
      cur.push_back(c);
    }
  }
  flush();
  return out;
}

#if defined(_WIN32)
static std::string ToLower(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

static std::string Trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  return value.substr(start);
}

static bool LooksLikeIpv4(const std::string& value) {
  IN_ADDR addr{};
  return InetPtonA(AF_INET, value.c_str(), &addr) == 1;
}

static bool LooksLikeIpv6(const std::string& value) {
  IN6_ADDR addr6{};
  return InetPtonA(AF_INET6, value.c_str(), &addr6) == 1;
}

static bool LooksLikeIpAddress(const std::string& value) {
  return LooksLikeIpv4(value) || LooksLikeIpv6(value);
}

static std::string NormalizeDnsName(std::string value) {
  value = ToLower(Trim(std::move(value)));
  while (!value.empty() && value.back() == '.') {
    value.pop_back();
  }
  return value;
}

static std::vector<std::string> SplitSanEntries(const std::string& value) {
  std::vector<std::string> entries = SplitIps(value);
  std::vector<std::string> out;
  std::set<std::string> seen;
  for (auto& item : entries) {
    item = Trim(std::move(item));
    if (item.empty()) continue;
    const std::string normalized = LooksLikeIpAddress(item) ? item : NormalizeDnsName(item);
    if (normalized.empty()) continue;
    if (seen.insert(normalized).second) {
      out.push_back(normalized);
    }
  }
  return out;
}

static std::wstring Utf8ToWide(const std::string& s) {
  if (s.empty()) return L"";
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  if (n <= 0) return L"";
  std::wstring w(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
  return w;
}
static std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) return "";
  int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  if (n <= 0) return "";
  std::string s(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), n, nullptr, nullptr);
  return s;
}

static bool FileExistsW(const std::wstring& p) {
  DWORD attr = GetFileAttributesW(p.c_str());
  return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

static std::wstring GetExecutableDir() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
  if (n == 0 || n >= std::size(buf)) return L"";
  fs::path exePath(buf);
  return exePath.parent_path().wstring();
}

static std::wstring FindOpenSsl(std::string& err) {
  // 1) OPENSSL_EXE
  {
    wchar_t buf[1024];
    DWORD n = GetEnvironmentVariableW(L"OPENSSL_EXE", buf, (DWORD)std::size(buf));
    if (n > 0) {
      std::wstring p(buf);
      if (FileExistsW(p)) return p;
    }
  }

  // 2) Common paths relative to the running executable.
  {
    const fs::path exeDir = GetExecutableDir();
    if (!exeDir.empty()) {
      const fs::path candidates[] = {
        exeDir / "openssl.exe",
        exeDir / "tools" / "openssl" / "openssl.exe",
        exeDir / "tools" / "openssl.exe"
      };
      for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) return candidate.wstring();
      }
    }
  }

  // 3) Search PATH
  {
    wchar_t out[MAX_PATH];
    DWORD n = SearchPathW(nullptr, L"openssl.exe", nullptr, (DWORD)std::size(out), out, nullptr);
    if (n > 0 && n < std::size(out)) {
      return std::wstring(out);
    }
  }

  // 4) Common install locations
  const wchar_t* candidates[] = {
    L"C:\\Program Files\\OpenSSL-Win64\\bin\\openssl.exe",
    L"C:\\Program Files\\OpenSSL-Win32\\bin\\openssl.exe",
    L"C:\\OpenSSL-Win64\\bin\\openssl.exe",
    L"C:\\OpenSSL-Win32\\bin\\openssl.exe",
  };
  for (auto* c : candidates) {
    if (FileExistsW(c)) return c;
  }

  // 5) vcpkg tools
  {
    wchar_t buf[1024];
    DWORD n = GetEnvironmentVariableW(L"VCPKG_ROOT", buf, (DWORD)std::size(buf));
    if (n > 0) {
      fs::path root = fs::path(buf);
      fs::path p1 = root / "installed" / "x64-windows" / "tools" / "openssl" / "openssl.exe";
      fs::path p2 = root / "installed" / "x64-windows-static" / "tools" / "openssl" / "openssl.exe";
      fs::path p3 = root / "installed" / "x64-windows" / "tools" / "openssl" / "bin" / "openssl.exe";
      fs::path p4 = root / "installed" / "x64-windows-static" / "tools" / "openssl" / "bin" / "openssl.exe";
      if (fs::exists(p1)) return p1.wstring();
      if (fs::exists(p2)) return p2.wstring();
      if (fs::exists(p3)) return p3.wstring();
      if (fs::exists(p4)) return p4.wstring();
    }
  }

  err = "openssl.exe not found. Install OpenSSL or set OPENSSL_EXE env var.";
  return L"";
}

static std::string JoinValues(const std::vector<std::string>& values) {
  std::ostringstream ss;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) ss << ", ";
    ss << values[i];
  }
  return ss.str();
}

#if defined(LAN_CERT_USE_OPENSSL_API)
static std::string ChooseCommonName(const std::vector<std::string>& sans) {
  for (const auto& entry : sans) {
    if (!LooksLikeIpAddress(entry)) return entry;
  }
  for (const auto& entry : sans) {
    if (LooksLikeIpv4(entry)) return entry;
  }
  if (!sans.empty()) return sans.front();
  return "LAN Screen Share";
}

static std::string BuildSubjectAltNameValue(const std::vector<std::string>& sans) {
  std::ostringstream ss;
  for (std::size_t i = 0; i < sans.size(); ++i) {
    if (i != 0) ss << ",";
    ss << (LooksLikeIpAddress(sans[i]) ? "IP:" : "DNS:") << sans[i];
  }
  return ss.str();
}

static bool AddX509Extension(X509* cert,
                             X509V3_CTX* ctx,
                             int nid,
                             const std::string& value,
                             std::string& err) {
  X509_EXTENSION* ext = X509V3_EXT_conf_nid(nullptr, ctx, nid, const_cast<char*>(value.c_str()));
  if (!ext) {
    err = "OpenSSL failed to create X509 extension nid=" + std::to_string(nid);
    return false;
  }
  const int addOk = X509_add_ext(cert, ext, -1);
  X509_EXTENSION_free(ext);
  if (addOk != 1) {
    err = "OpenSSL failed to append X509 extension nid=" + std::to_string(nid);
    return false;
  }
  return true;
}

static bool GenerateSelfSignedWithOpenSslApi(const std::string& keyFile,
                                             const std::string& certFile,
                                             const std::vector<std::string>& sans,
                                             std::string& err) {
  EVP_PKEY_CTX* pkeyCtx = nullptr;
  EVP_PKEY* pkey = nullptr;
  X509* cert = nullptr;
  FILE* keyFp = nullptr;
  FILE* certFp = nullptr;

  auto cleanup = [&]() {
    if (keyFp) fclose(keyFp);
    if (certFp) fclose(certFp);
    if (cert) X509_free(cert);
    if (pkey) EVP_PKEY_free(pkey);
    if (pkeyCtx) EVP_PKEY_CTX_free(pkeyCtx);
  };

  pkeyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
  if (!pkeyCtx) {
    err = "OpenSSL failed to allocate EVP_PKEY context.";
    cleanup();
    return false;
  }
  if (EVP_PKEY_keygen_init(pkeyCtx) <= 0 ||
      EVP_PKEY_CTX_set_rsa_keygen_bits(pkeyCtx, 2048) <= 0 ||
      EVP_PKEY_keygen(pkeyCtx, &pkey) <= 0) {
    err = "OpenSSL failed to generate RSA private key.";
    cleanup();
    return false;
  }

  cert = X509_new();
  if (!cert) {
    err = "OpenSSL failed to allocate X509 certificate.";
    cleanup();
    return false;
  }

  if (X509_set_version(cert, 2) != 1) {
    err = "OpenSSL failed to set X509 version.";
    cleanup();
    return false;
  }
  ASN1_INTEGER_set(X509_get_serialNumber(cert), static_cast<long>(std::time(nullptr)));
  X509_gmtime_adj(X509_getm_notBefore(cert), 0);
  X509_gmtime_adj(X509_getm_notAfter(cert), 60L * 60L * 24L * 3650L);
  if (X509_set_pubkey(cert, pkey) != 1) {
    err = "OpenSSL failed to attach public key to certificate.";
    cleanup();
    return false;
  }

  X509_NAME* subject = X509_get_subject_name(cert);
  const std::string commonName = ChooseCommonName(sans);
  if (!subject ||
      X509_NAME_add_entry_by_txt(subject,
                                 "CN",
                                 MBSTRING_ASC,
                                 reinterpret_cast<const unsigned char*>(commonName.c_str()),
                                 -1,
                                 -1,
                                 0) != 1) {
    err = "OpenSSL failed to set certificate subject CN.";
    cleanup();
    return false;
  }
  if (X509_set_issuer_name(cert, subject) != 1) {
    err = "OpenSSL failed to set certificate issuer.";
    cleanup();
    return false;
  }

  X509V3_CTX ctx{};
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, cert, cert, nullptr, nullptr, 0);

  if (!AddX509Extension(cert, &ctx, NID_basic_constraints, "critical,CA:FALSE", err) ||
      !AddX509Extension(cert, &ctx, NID_key_usage, "critical,digitalSignature,keyEncipherment", err) ||
      !AddX509Extension(cert, &ctx, NID_ext_key_usage, "serverAuth", err) ||
      !AddX509Extension(cert, &ctx, NID_subject_alt_name, BuildSubjectAltNameValue(sans), err)) {
    cleanup();
    return false;
  }

  if (X509_sign(cert, pkey, EVP_sha256()) <= 0) {
    err = "OpenSSL failed to sign certificate.";
    cleanup();
    return false;
  }

  keyFp = _wfopen(Utf8ToWide(keyFile).c_str(), L"wb");
  if (!keyFp) {
    err = "failed to open private key output file";
    cleanup();
    return false;
  }
  if (PEM_write_PrivateKey(keyFp, pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1) {
    err = "OpenSSL failed to write PEM private key.";
    cleanup();
    return false;
  }
  fclose(keyFp);
  keyFp = nullptr;

  certFp = _wfopen(Utf8ToWide(certFile).c_str(), L"wb");
  if (!certFp) {
    err = "failed to open certificate output file";
    cleanup();
    return false;
  }
  if (PEM_write_X509(certFp, cert) != 1) {
    err = "OpenSSL failed to write PEM certificate.";
    cleanup();
    return false;
  }

  cleanup();
  err.clear();
  return true;
}
#endif

static bool InspectCertificateInternal(const std::string& certFile,
                                       const std::string& keyFile,
                                       const std::string& sanIp,
                                       CertStatus& status,
                                       std::string& err) {
  status = {};
  status.certExists = fs::exists(certFile);
  status.keyExists = fs::exists(keyFile);
  status.expectedAltNames = SplitSanEntries(sanIp);

  if (!status.certExists || !status.keyExists) {
    status.detail = !status.certExists && !status.keyExists
        ? "Certificate and key files are missing."
        : (!status.certExists ? "Certificate file is missing." : "Certificate key file is missing.");
    err.clear();
    return true;
  }

  const std::wstring certPath = Utf8ToWide(certFile);
  DWORD encoding = 0;
  DWORD contentType = 0;
  DWORD formatType = 0;
  HCERTSTORE store = nullptr;
  HCRYPTMSG message = nullptr;
  const void* context = nullptr;

  const BOOL ok = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                   certPath.c_str(),
                                   CERT_QUERY_CONTENT_FLAG_CERT,
                                   CERT_QUERY_FORMAT_FLAG_ALL,
                                   0,
                                   &encoding,
                                   &contentType,
                                   &formatType,
                                   &store,
                                   &message,
                                   &context);
  if (!ok || !context) {
    err = "CryptQueryObject failed for certificate file.";
    status.detail = "Certificate file exists, but Windows could not parse it.";
    if (store) CertCloseStore(store, 0);
    if (message) CryptMsgClose(message);
    return false;
  }

  auto* certCtx = static_cast<PCCERT_CONTEXT>(context);
  status.certParsable = true;

  FILETIME now{};
  GetSystemTimeAsFileTime(&now);
  status.validNow =
      CompareFileTime(&certCtx->pCertInfo->NotBefore, &now) <= 0 &&
      CompareFileTime(&certCtx->pCertInfo->NotAfter, &now) >= 0;

  const PCERT_EXTENSION ext = CertFindExtension(szOID_SUBJECT_ALT_NAME2,
                                                certCtx->pCertInfo->cExtension,
                                                certCtx->pCertInfo->rgExtension);
  if (ext) {
    CERT_ALT_NAME_INFO* altInfo = nullptr;
    DWORD altSize = 0;
    if (CryptDecodeObjectEx(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
                            X509_ALTERNATE_NAME,
                            ext->Value.pbData,
                            ext->Value.cbData,
                            CRYPT_DECODE_ALLOC_FLAG,
                            nullptr,
                            &altInfo,
                            &altSize) && altInfo) {
      for (DWORD i = 0; i < altInfo->cAltEntry; ++i) {
        const CERT_ALT_NAME_ENTRY& entry = altInfo->rgAltEntry[i];
        if (entry.dwAltNameChoice == CERT_ALT_NAME_DNS_NAME && entry.pwszDNSName) {
          status.presentDnsSans.push_back(NormalizeDnsName(WideToUtf8(entry.pwszDNSName)));
        } else if (entry.dwAltNameChoice == CERT_ALT_NAME_IP_ADDRESS && entry.IPAddress.cbData > 0) {
          char ipBuf[INET6_ADDRSTRLEN]{};
          if (entry.IPAddress.cbData == 4 &&
              InetNtopA(AF_INET, entry.IPAddress.pbData, ipBuf, static_cast<DWORD>(std::size(ipBuf)))) {
            status.presentIpSans.push_back(ipBuf);
          } else if (entry.IPAddress.cbData == 16 &&
                     InetNtopA(AF_INET6, entry.IPAddress.pbData, ipBuf, static_cast<DWORD>(std::size(ipBuf)))) {
            status.presentIpSans.push_back(ToLower(ipBuf));
          }
        }
      }
      LocalFree(altInfo);
    }
  }

  for (const auto& expected : status.expectedAltNames) {
    const bool isIp = LooksLikeIpAddress(expected);
    const auto& present = isIp ? status.presentIpSans : status.presentDnsSans;
    const bool matched = std::find(present.begin(), present.end(), expected) != present.end();
    if (!matched) {
      status.missingAltNames.push_back(expected);
    }
  }

  status.sanMatches = status.missingAltNames.empty();
  status.ready = status.certParsable && status.validNow && status.sanMatches;

  if (!status.certParsable) {
    status.detail = "Certificate file is not parseable.";
  } else if (!status.validNow) {
    status.detail = "Certificate is expired or not yet valid.";
  } else if (!status.sanMatches) {
    status.detail = "Certificate SAN does not match the current host entries. Missing: " + JoinValues(status.missingAltNames);
  } else {
    status.detail = "Certificate matches the current host entries.";
  }

  CertFreeCertificateContext(certCtx);
  if (store) CertCloseStore(store, 0);
  if (message) CryptMsgClose(message);
  err.clear();
  return true;
}

static bool RunProcessCapture(const std::wstring& cmdLine, const std::wstring& workdir, std::string& out, std::string& errOut) {
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  HANDLE hOutRead = nullptr, hOutWrite = nullptr;
  if (!CreatePipe(&hOutRead, &hOutWrite, &sa, 0)) return false;
  SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);

  HANDLE hErrRead = nullptr, hErrWrite = nullptr;
  if (!CreatePipe(&hErrRead, &hErrWrite, &sa, 0)) {
    CloseHandle(hOutRead);
    CloseHandle(hOutWrite);
    return false;
  }
  SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = hOutWrite;
  si.hStdError = hErrWrite;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

  PROCESS_INFORMATION pi{};

  std::vector<wchar_t> mutableCmd(cmdLine.begin(), cmdLine.end());
  mutableCmd.push_back(L'\0');

  BOOL ok = CreateProcessW(
    nullptr,
    mutableCmd.data(),
    nullptr,
    nullptr,
    TRUE,
    CREATE_NO_WINDOW,
    nullptr,
    workdir.empty() ? nullptr : workdir.c_str(),
    &si,
    &pi);

  CloseHandle(hOutWrite);
  CloseHandle(hErrWrite);

  if (!ok) {
    CloseHandle(hOutRead);
    CloseHandle(hErrRead);
    return false;
  }

  auto readAll = [](HANDLE h) {
    std::string s;
    char buf[1024];
    DWORD read = 0;
    while (ReadFile(h, buf, sizeof(buf), &read, nullptr) && read > 0) {
      s.append(buf, buf + read);
    }
    return s;
  };

  WaitForSingleObject(pi.hProcess, 60000);
  out = readAll(hOutRead);
  errOut = readAll(hErrRead);

  CloseHandle(hOutRead);
  CloseHandle(hErrRead);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  return true;
}
#endif

bool CertManager::EnsureSelfSigned(const std::string& outDir, const std::string& sanIp, CertPaths& paths, std::string& err) {
  fs::create_directories(outDir);

  paths.keyFile = (fs::path(outDir) / "server.key").string();
  paths.certFile = (fs::path(outDir) / "server.crt").string();

  CertStatus current{};
  std::string inspectErr;
  if (InspectCertificate(paths.certFile, paths.keyFile, sanIp, current, inspectErr) && current.ready) {
    err.clear();
    return true;
  }

  std::vector<std::string> sans = SplitSanEntries(sanIp);
  if (sans.empty()) {
    sans.push_back("127.0.0.1");
    sans.push_back("localhost");
  } else {
    if (std::find(sans.begin(), sans.end(), "127.0.0.1") == sans.end()) {
      sans.push_back("127.0.0.1");
    }
    if (std::find(sans.begin(), sans.end(), "localhost") == sans.end()) {
      sans.push_back("localhost");
    }
  }

  std::error_code removeEc;
  fs::remove(paths.keyFile, removeEc);
  removeEc.clear();
  fs::remove(paths.certFile, removeEc);

  // Create openssl config with SAN.
  fs::path cnf = fs::path(outDir) / "openssl_san.cnf";
  {
    std::ofstream f(cnf, std::ios::binary);
    if (!f) {
      err = "failed to write openssl config: " + cnf.string();
      return false;
    }

    f << "[ req ]\n";
    f << "default_bits = 2048\n";
    f << "prompt = no\n";
    f << "default_md = sha256\n";
    f << "x509_extensions = v3_req\n";
    f << "distinguished_name = dn\n\n";

    f << "[ dn ]\n";
    f << "CN = LAN Screen Share\n\n";

    f << "[ v3_req ]\n";
    f << "subjectAltName = @alt_names\n";
    f << "keyUsage = digitalSignature, keyEncipherment\n";
    f << "extendedKeyUsage = serverAuth\n\n";

    f << "[ alt_names ]\n";
    std::size_t ipIndex = 1;
    std::size_t dnsIndex = 1;
    for (const auto& entry : sans) {
      if (LooksLikeIpAddress(entry)) {
        f << "IP." << ipIndex++ << " = " << entry << "\n";
      } else {
        f << "DNS." << dnsIndex++ << " = " << entry << "\n";
      }
    }
  }

#if defined(LAN_CERT_USE_OPENSSL_API)
  if (!GenerateSelfSignedWithOpenSslApi(paths.keyFile, paths.certFile, sans, err)) {
    return false;
  }
#elif defined(_WIN32)
  std::string ferr;
  std::wstring openssl = FindOpenSsl(ferr);
  if (openssl.empty()) {
    err = ferr;
    return false;
  }

  // openssl req -x509 -nodes -newkey rsa:2048 -keyout server.key -out server.crt -days 3650 -config openssl_san.cnf
  std::wstringstream cmd;
  cmd << L"\"" << openssl << L"\"";
  cmd << L" req -x509 -nodes -newkey rsa:2048";
  cmd << L" -keyout \"" << Utf8ToWide(paths.keyFile) << L"\"";
  cmd << L" -out \"" << Utf8ToWide(paths.certFile) << L"\"";
  cmd << L" -days 3650";
  cmd << L" -config \"" << cnf.wstring() << L"\"";

  std::string out, errOut;
  bool ok = RunProcessCapture(cmd.str(), Utf8ToWide(outDir), out, errOut);
  if (!ok) {
    err = "failed to run openssl";
    return false;
  }

  if (!fs::exists(paths.keyFile) || !fs::exists(paths.certFile)) {
    err = "openssl did not produce expected files. stdout=" + out + " stderr=" + errOut;
    return false;
  }
#else
  // Non-Windows: attempt to call system openssl.
  // (Best effort; keep MVP simple.)
  std::stringstream cmd;
  cmd << "openssl req -x509 -nodes -newkey rsa:2048";
  cmd << " -keyout \"" << paths.keyFile << "\"";
  cmd << " -out \"" << paths.certFile << "\"";
  cmd << " -days 3650";
  cmd << " -config \"" << cnf.string() << "\"";
  int rc = std::system(cmd.str().c_str());
  if (rc != 0) {
    err = "openssl system call failed (rc=" + std::to_string(rc) + ")";
    return false;
  }
  if (!fs::exists(paths.keyFile) || !fs::exists(paths.certFile)) {
    err = "openssl did not produce expected files";
    return false;
  }
#endif

  CertStatus finalStatus{};
  if (!InspectCertificate(paths.certFile, paths.keyFile, sanIp, finalStatus, inspectErr)) {
    err = inspectErr.empty() ? "Certificate generation succeeded, but validation failed." : inspectErr;
    return false;
  }
  if (!finalStatus.ready) {
    err = finalStatus.detail.empty() ? "Generated certificate does not match the requested SANs." : finalStatus.detail;
    return false;
  }

  return true;
}

bool CertManager::InspectCertificate(const std::string& certFile,
                                     const std::string& keyFile,
                                     const std::string& sanIp,
                                     CertStatus& status,
                                     std::string& err) {
#if defined(_WIN32)
  return InspectCertificateInternal(certFile, keyFile, sanIp, status, err);
#else
  status = {};
  status.certExists = fs::exists(certFile);
  status.keyExists = fs::exists(keyFile);
  status.certParsable = status.certExists;
  status.validNow = status.certExists;
  status.sanMatches = status.certExists;
  status.ready = status.certExists && status.keyExists;
  status.detail = status.ready ? "Certificate file exists." : "Certificate or key file is missing.";
  err.clear();
  return true;
#endif
}

} // namespace lan::cert
