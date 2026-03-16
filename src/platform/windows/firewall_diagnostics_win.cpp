#include "platform/windows/firewall_diagnostics_win.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <netfw.h>
#include <oleauto.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "FirewallAPI.lib")
#endif

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>

namespace lan::platform::windows {
#if defined(_WIN32)
namespace {

std::wstring ToLowerCopy(std::wstring value) {
  std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
    return static_cast<wchar_t>(std::towlower(ch));
  });
  return value;
}

std::wstring NormalizePath(const std::filesystem::path& path) {
  try {
    return ToLowerCopy(std::filesystem::weakly_canonical(path).wstring());
  } catch (...) {
    return ToLowerCopy(path.lexically_normal().wstring());
  }
}

std::wstring NormalizePathString(std::wstring value) {
  if (value.empty()) return {};
  try {
    return NormalizePath(std::filesystem::path(value));
  } catch (...) {
    return ToLowerCopy(std::move(value));
  }
}

void SafeRelease(IUnknown* ptr) {
  if (ptr) ptr->Release();
}

struct ScopedVariant {
  VARIANT value{};
  ScopedVariant() { VariantInit(&value); }
  ~ScopedVariant() { VariantClear(&value); }
};

std::wstring BstrToWString(BSTR value) {
  if (!value) return {};
  return std::wstring(value, SysStringLen(value));
}

bool ProfileMatches(long ruleProfiles, long activeProfiles) {
  constexpr long kAllProfiles = NET_FW_PROFILE2_ALL;
  if (ruleProfiles == 0 || ruleProfiles == kAllProfiles) return true;
  return (ruleProfiles & activeProfiles) != 0;
}

bool ParsePortToken(std::wstring token, int port) {
  token.erase(std::remove_if(token.begin(), token.end(), [](wchar_t ch) { return std::iswspace(ch); }), token.end());
  if (token.empty()) return false;
  if (token == L"*" || token == L"Any") return true;
  const auto dash = token.find(L'-');
  if (dash != std::wstring::npos) {
    const int start = _wtoi(token.substr(0, dash).c_str());
    const int end = _wtoi(token.substr(dash + 1).c_str());
    return start > 0 && end >= start && port >= start && port <= end;
  }
  return _wtoi(token.c_str()) == port;
}

bool LocalPortsContain(BSTR ports, int port) {
  const std::wstring value = BstrToWString(ports);
  if (value.empty()) return false;
  std::size_t start = 0;
  while (start < value.size()) {
    const auto comma = value.find(L',', start);
    const auto token = value.substr(start, comma == std::wstring::npos ? std::wstring::npos : comma - start);
    if (ParsePortToken(token, port)) return true;
    if (comma == std::wstring::npos) break;
    start = comma + 1;
  }
  return false;
}

std::wstring ActiveProfileLabel(long profiles) {
  std::vector<std::wstring> labels;
  if ((profiles & NET_FW_PROFILE2_DOMAIN) != 0) labels.push_back(L"Domain");
  if ((profiles & NET_FW_PROFILE2_PRIVATE) != 0) labels.push_back(L"Private");
  if ((profiles & NET_FW_PROFILE2_PUBLIC) != 0) labels.push_back(L"Public");
  if (labels.empty()) return L"Unknown";
  std::wstringstream ss;
  for (std::size_t i = 0; i < labels.size(); ++i) {
    if (i != 0) ss << L", ";
    ss << labels[i];
  }
  return ss.str();
}

} // namespace
#endif

FirewallProbeResult ProbeFirewallReadiness(const std::filesystem::path& serverExePath,
                                          int port) {
  FirewallProbeResult result;
#if defined(_WIN32)
  if (port <= 0 || port > 65535) {
    result.detail = L"Firewall readiness was skipped because the configured TCP port is invalid.";
    return result;
  }

  HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  const bool shouldUninit = SUCCEEDED(initHr);
  if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) {
    result.detail = L"Firewall readiness check failed: COM initialization failed.";
    return result;
  }

  INetFwPolicy2* policy = nullptr;
  HRESULT hr = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER,
                                __uuidof(INetFwPolicy2), reinterpret_cast<void**>(&policy));
  if (FAILED(hr) || !policy) {
    if (shouldUninit) CoUninitialize();
    result.detail = L"Firewall readiness check failed: Windows Firewall policy is unavailable.";
    return result;
  }

  long currentProfiles = 0;
  hr = policy->get_CurrentProfileTypes(&currentProfiles);
  if (FAILED(hr)) {
    SafeRelease(policy);
    if (shouldUninit) CoUninitialize();
    result.detail = L"Firewall readiness check failed: active firewall profiles could not be read.";
    return result;
  }

  const long profileMask = currentProfiles == 0 ? NET_FW_PROFILE2_ALL : currentProfiles;
  const long profilesToInspect[] = {NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC};
  for (long profile : profilesToInspect) {
    if ((profileMask & profile) == 0) continue;
    VARIANT_BOOL enabled = VARIANT_FALSE;
    if (SUCCEEDED(policy->get_FirewallEnabled(profile, &enabled)) && enabled == VARIANT_TRUE) {
      result.firewallEnabled = true;
    }
  }

  INetFwRules* rules = nullptr;
  hr = policy->get_Rules(&rules);
  if (SUCCEEDED(hr) && rules) {
    IUnknown* unk = nullptr;
    hr = rules->get__NewEnum(&unk);
    if (SUCCEEDED(hr) && unk) {
      IEnumVARIANT* enumerator = nullptr;
      hr = unk->QueryInterface(IID_PPV_ARGS(&enumerator));
      if (SUCCEEDED(hr) && enumerator) {
        const std::wstring normalizedExe = NormalizePath(serverExePath);
        while (!result.matchingAppRule || !result.matchingPortRule) {
          ScopedVariant variant;
          ULONG fetched = 0;
          if (enumerator->Next(1, &variant.value, &fetched) != S_OK || fetched == 0) break;
          if (variant.value.vt != VT_DISPATCH && variant.value.vt != VT_UNKNOWN) continue;

          INetFwRule* rule = nullptr;
          IUnknown* candidate = variant.value.vt == VT_DISPATCH
              ? reinterpret_cast<IUnknown*>(variant.value.pdispVal)
              : variant.value.punkVal;
          if (!candidate) continue;
          if (FAILED(candidate->QueryInterface(__uuidof(INetFwRule), reinterpret_cast<void**>(&rule))) || !rule) {
            continue;
          }

          VARIANT_BOOL enabled = VARIANT_FALSE;
          long direction = 0;
          long action = 0;
          long ruleProfiles = 0;
          if (FAILED(rule->get_Enabled(&enabled)) || enabled != VARIANT_TRUE ||
              FAILED(rule->get_Direction(&direction)) || direction != NET_FW_RULE_DIR_IN ||
              FAILED(rule->get_Action(&action)) || action != NET_FW_ACTION_ALLOW ||
              FAILED(rule->get_Profiles(&ruleProfiles)) || !ProfileMatches(ruleProfiles, profileMask)) {
            rule->Release();
            continue;
          }

          if (!result.matchingAppRule) {
            BSTR appName = nullptr;
            if (SUCCEEDED(rule->get_ApplicationName(&appName)) && appName) {
              const std::wstring normalizedRulePath = NormalizePathString(BstrToWString(appName));
              if (!normalizedRulePath.empty() && !normalizedExe.empty() && normalizedRulePath == normalizedExe) {
                result.matchingAppRule = true;
              }
            }
            if (appName) SysFreeString(appName);
          }

          if (!result.matchingPortRule) {
            long protocol = NET_FW_IP_PROTOCOL_ANY;
            if (SUCCEEDED(rule->get_Protocol(&protocol)) &&
                (protocol == NET_FW_IP_PROTOCOL_TCP || protocol == NET_FW_IP_PROTOCOL_ANY)) {
              BSTR localPorts = nullptr;
              if (SUCCEEDED(rule->get_LocalPorts(&localPorts)) && localPorts) {
                if (LocalPortsContain(localPorts, port)) {
                  result.matchingPortRule = true;
                }
              }
              if (localPorts) SysFreeString(localPorts);
            }
          }

          rule->Release();
        }
        enumerator->Release();
      }
      unk->Release();
    }
    rules->Release();
  }

  policy->Release();
  if (shouldUninit) CoUninitialize();

  result.ready = !result.firewallEnabled || result.matchingAppRule || result.matchingPortRule;
  std::wstringstream detail;
  if (!result.firewallEnabled) {
    detail << L"Windows Firewall is not enabled for the current profile(s): " << ActiveProfileLabel(profileMask)
           << L". Inbound viewer traffic is not blocked by Windows Firewall itself.";
  } else if (result.ready) {
    detail << L"Windows Firewall is enabled for " << ActiveProfileLabel(profileMask) << L", and an inbound allow rule was detected";
    if (result.matchingAppRule && result.matchingPortRule) {
      detail << L" for both the server executable and TCP port " << port;
    } else if (result.matchingAppRule) {
      detail << L" for the server executable";
    } else {
      detail << L" for TCP port " << port;
    }
    detail << L".";
  } else {
    detail << L"Windows Firewall is enabled for " << ActiveProfileLabel(profileMask)
           << L", but no enabled inbound allow rule was detected for the server executable or TCP port " << port << L".";
  }
  result.detail = detail.str();
#else
  (void)serverExePath;
  (void)port;
  result.detail = L"Firewall readiness is only implemented on Windows.";
#endif
  return result;
}

} // namespace lan::platform::windows
