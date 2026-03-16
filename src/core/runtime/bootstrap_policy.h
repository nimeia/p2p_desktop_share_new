#pragma once

#include <string>
#include <string_view>

namespace lan::runtime {

bool IsLoopbackHost(std::wstring_view host);
bool IsPrivateLanHost(std::wstring_view host);
bool ShouldBypassLocalCertificateForHost(std::wstring_view host);
bool ShouldBypassLocalCertificateForUrl(std::wstring_view url);
std::wstring DescribeLocalCertificateBypassPolicy();

} // namespace lan::runtime
