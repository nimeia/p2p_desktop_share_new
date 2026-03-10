#include "cert_manager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(_WIN32)
  #include <windows.h>
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

  if (fs::exists(paths.keyFile) && fs::exists(paths.certFile)) {
    return true;
  }

  // Build SAN list (comma/space separated supported).
  auto ips = SplitIps(sanIp);
  if (ips.empty()) {
    ips.push_back("127.0.0.1");
  }

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
    for (size_t i = 0; i < ips.size(); ++i) {
      f << "IP." << (i + 1) << " = " << ips[i] << "\n";
    }
  }

#if defined(_WIN32)
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

  return true;
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
  return true;
#endif
}

} // namespace lan::cert
