#include "core/runtime/bootstrap_policy.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>

namespace lan::runtime {
namespace {

std::wstring Trim(std::wstring_view value) {
  std::size_t start = 0;
  while (start < value.size() && std::iswspace(value[start])) ++start;
  std::size_t end = value.size();
  while (end > start && std::iswspace(value[end - 1])) --end;
  return std::wstring(value.substr(start, end - start));
}

std::wstring ToLower(std::wstring_view value) {
  std::wstring out(value.begin(), value.end());
  std::transform(out.begin(), out.end(), out.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return out;
}

std::wstring StripIpv6Brackets(std::wstring_view host) {
  if (host.size() >= 2 && host.front() == L'[' && host.back() == L']') {
    return std::wstring(host.substr(1, host.size() - 2));
  }
  return std::wstring(host);
}

bool ParseIpv4(std::wstring_view host, std::array<int, 4>& octets) {
  std::array<int, 4> parsed{};
  std::size_t begin = 0;
  for (std::size_t index = 0; index < parsed.size(); ++index) {
    if (begin >= host.size()) return false;
    std::size_t end = host.find(L'.', begin);
    if (end == std::wstring_view::npos) end = host.size();
    if (end == begin) return false;
    if (index + 1 < parsed.size() && end == host.size()) return false;
    if (index + 1 == parsed.size() && end != host.size()) return false;

    int value = 0;
    for (std::size_t pos = begin; pos < end; ++pos) {
      if (!std::iswdigit(host[pos])) return false;
      value = (value * 10) + static_cast<int>(host[pos] - L'0');
      if (value > 255) return false;
    }
    parsed[index] = value;
    begin = end + 1;
  }
  octets = parsed;
  return true;
}

std::wstring ExtractHostFromUrl(std::wstring_view url) {
  const std::size_t schemePos = url.find(L"://");
  if (schemePos == std::wstring_view::npos) {
    return Trim(url);
  }

  std::size_t hostStart = schemePos + 3;
  if (hostStart >= url.size()) return {};

  std::size_t hostEnd = hostStart;
  if (url[hostStart] == L'[') {
    const std::size_t closing = url.find(L']', hostStart + 1);
    if (closing == std::wstring_view::npos) return {};
    hostEnd = closing + 1;
  } else {
    while (hostEnd < url.size()) {
      const wchar_t ch = url[hostEnd];
      if (ch == L':' || ch == L'/' || ch == L'?' || ch == L'#') break;
      ++hostEnd;
    }
  }
  return Trim(url.substr(hostStart, hostEnd - hostStart));
}

} // namespace

bool IsLoopbackHost(std::wstring_view host) {
  const std::wstring normalized = ToLower(StripIpv6Brackets(Trim(host)));
  return normalized == L"localhost" || normalized == L"127.0.0.1" || normalized == L"::1";
}

bool IsPrivateLanHost(std::wstring_view host) {
  const std::wstring normalized = ToLower(StripIpv6Brackets(Trim(host)));
  if (normalized.empty()) return false;
  if (IsLoopbackHost(normalized)) return true;
  if (normalized == L"::1") return true;
  if (normalized.rfind(L"fe80:", 0) == 0 || normalized.rfind(L"fc", 0) == 0 || normalized.rfind(L"fd", 0) == 0) {
    return true;
  }

  std::array<int, 4> octets{};
  if (!ParseIpv4(normalized, octets)) return false;
  if (octets[0] == 10) return true;
  if (octets[0] == 192 && octets[1] == 168) return true;
  if (octets[0] == 172 && octets[1] >= 16 && octets[1] <= 31) return true;
  if (octets[0] == 169 && octets[1] == 254) return true;
  return false;
}

bool ShouldBypassLocalCertificateForHost(std::wstring_view host) {
  return IsPrivateLanHost(host);
}

bool ShouldBypassLocalCertificateForUrl(std::wstring_view url) {
  return ShouldBypassLocalCertificateForHost(ExtractHostFromUrl(url));
}

std::wstring DescribeLocalCertificateBypassPolicy() {
  return L"plain-http-first";
}

} // namespace lan::runtime
