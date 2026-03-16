#include "core/runtime/bootstrap_policy.h"

#include <iostream>
#include <stdexcept>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

} // namespace

int main() {
  using lan::runtime::DescribeLocalCertificateBypassPolicy;
  using lan::runtime::IsLoopbackHost;
  using lan::runtime::IsPrivateLanHost;
  using lan::runtime::ShouldBypassLocalCertificateForUrl;

  Expect(IsLoopbackHost(L"localhost"), "localhost should be treated as loopback");
  Expect(IsLoopbackHost(L"127.0.0.1"), "127.0.0.1 should be treated as loopback");
  Expect(IsPrivateLanHost(L"10.0.0.25"), "10.x should be treated as private LAN");
  Expect(IsPrivateLanHost(L"172.20.10.4"), "172.16/12 should be treated as private LAN");
  Expect(IsPrivateLanHost(L"192.168.137.1"), "192.168.x should be treated as private LAN");
  Expect(IsPrivateLanHost(L"169.254.22.7"), "link-local IPv4 should be treated as local LAN");
  Expect(!IsPrivateLanHost(L"8.8.8.8"), "public IPv4 should not be treated as private LAN");
  Expect(ShouldBypassLocalCertificateForUrl(L"https://127.0.0.1:9443/host?room=a"), "loopback URL should be bypass-eligible");
  Expect(ShouldBypassLocalCertificateForUrl(L"https://192.168.1.77:9443/view?room=a"), "private LAN URL should be bypass-eligible");
  Expect(!ShouldBypassLocalCertificateForUrl(L"https://example.com:9443/view?room=a"), "public hostnames should not be bypass-eligible");
  Expect(DescribeLocalCertificateBypassPolicy() == L"allow-loopback-and-private-lan-self-signed",
         "policy label should stay stable");

  std::cout << "bootstrap policy tests passed\n";
  return 0;
}
