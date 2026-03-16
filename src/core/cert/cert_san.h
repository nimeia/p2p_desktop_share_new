#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace lan::cert {

bool LooksLikeIpv4(std::string_view value);
bool LooksLikeIpv6(std::string_view value);
bool LooksLikeIpAddress(std::string_view value);
std::string NormalizeDnsName(std::string value);
std::string NormalizeSanEntry(std::string value);
std::vector<std::string> SplitSanEntries(std::string_view value);
std::vector<std::string> ExpandServerCertificateAltNames(std::string_view requestedAltNames);
std::string JoinValues(const std::vector<std::string>& values, std::string_view separator = ", ");

} // namespace lan::cert
