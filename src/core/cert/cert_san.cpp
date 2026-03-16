#include "core/cert/cert_san.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

#if defined(_WIN32)
  #include <winsock2.h>
  #include <ws2tcpip.h>
#else
  #include <arpa/inet.h>
#endif

namespace lan::cert {
namespace {

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string TrimAscii(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }
  return value.substr(start);
}

void PushUnique(std::vector<std::string>& out, std::set<std::string>& seen, std::string value) {
  if (value.empty()) return;
  if (seen.insert(value).second) {
    out.push_back(std::move(value));
  }
}

} // namespace

bool LooksLikeIpv4(std::string_view value) {
  if (value.empty()) return false;
  in_addr addr{};
  return inet_pton(AF_INET, std::string(value).c_str(), &addr) == 1;
}

bool LooksLikeIpv6(std::string_view value) {
  if (value.empty()) return false;
  in6_addr addr6{};
  return inet_pton(AF_INET6, std::string(value).c_str(), &addr6) == 1;
}

bool LooksLikeIpAddress(std::string_view value) {
  return LooksLikeIpv4(value) || LooksLikeIpv6(value);
}

std::string NormalizeDnsName(std::string value) {
  value = ToLowerAscii(TrimAscii(std::move(value)));
  while (!value.empty() && value.back() == '.') {
    value.pop_back();
  }
  return value;
}

std::string NormalizeSanEntry(std::string value) {
  value = TrimAscii(std::move(value));
  if (value.empty()) return {};
  return LooksLikeIpAddress(value) ? value : NormalizeDnsName(std::move(value));
}

std::vector<std::string> SplitSanEntries(std::string_view value) {
  std::vector<std::string> out;
  std::set<std::string> seen;
  std::string current;

  auto flush = [&]() {
    PushUnique(out, seen, NormalizeSanEntry(std::move(current)));
    current.clear();
  };

  for (char ch : value) {
    if (ch == ',' || ch == ';' || ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
      flush();
    } else {
      current.push_back(ch);
    }
  }
  flush();
  return out;
}

std::vector<std::string> ExpandServerCertificateAltNames(std::string_view requestedAltNames) {
  std::vector<std::string> out = SplitSanEntries(requestedAltNames);
  std::set<std::string> seen(out.begin(), out.end());
  PushUnique(out, seen, "127.0.0.1");
  PushUnique(out, seen, "localhost");
  return out;
}

std::string JoinValues(const std::vector<std::string>& values, std::string_view separator) {
  std::string out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) out += separator;
    out += values[i];
  }
  return out;
}

} // namespace lan::cert
