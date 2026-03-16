#pragma once

#include <cstddef>
#include <string>

namespace lan::runtime {

struct HostSessionRules {
  int defaultPort = 9443;
  std::wstring defaultBindAddress = L"0.0.0.0";
  std::wstring roomRule = L"letters / digits, 3-32 chars";
  std::wstring tokenRule = L"letters / digits, 6-64 chars";
  std::size_t generatedRoomLength = 6;
  std::size_t generatedTokenLength = 16;
};

struct HostSessionConfig {
  std::wstring bindAddress;
  int port = 0;
  std::wstring room;
  std::wstring token;
};

struct HostSessionFlags {
  bool viewerUrlCopied = false;
  bool shareBundleExported = false;
  bool shareWizardOpened = false;
  bool handoffStarted = false;
  bool handoffDelivered = false;
};

struct HostSessionState {
  HostSessionRules rules;
  HostSessionConfig config;
  HostSessionFlags flags;
};

struct HostSessionMutationResult {
  HostSessionState state;
  bool configChanged = false;
  bool generatedRoom = false;
  bool generatedToken = false;
  bool resetDeliveryStateApplied = false;
};

struct HostSessionAdminModel {
  std::wstring bindAddress;
  int port = 0;
  std::wstring room;
  std::wstring token;
  int defaultPort = 9443;
  std::wstring defaultBindAddress;
  std::wstring roomRule;
  std::wstring tokenRule;
  bool viewerUrlCopied = false;
  bool shareBundleExported = false;
  bool shareWizardOpened = false;
  bool handoffStarted = false;
  bool handoffDelivered = false;
};

HostSessionState NormalizeHostSessionState(const HostSessionState& state);
HostSessionMutationResult ApplyHostSessionConfig(const HostSessionState& current,
                                                std::wstring room,
                                                std::wstring token,
                                                std::wstring bindAddress,
                                                int port);
HostSessionMutationResult EnsureHostSessionCredentials(const HostSessionState& current);
HostSessionMutationResult GenerateHostSessionCredentials(const HostSessionState& current);
HostSessionAdminModel BuildHostSessionAdminModel(const HostSessionState& state);

} // namespace lan::runtime
