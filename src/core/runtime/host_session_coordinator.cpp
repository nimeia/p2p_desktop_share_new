#include "core/runtime/host_session_coordinator.h"

#include <algorithm>
#include <random>

namespace lan::runtime {
namespace {

std::wstring MakeRandomAlnum(std::size_t len) {
  static constexpr wchar_t kChars[] = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<std::size_t> dist(0, (sizeof(kChars) / sizeof(kChars[0])) - 2);

  std::wstring value;
  value.reserve(len);
  for (std::size_t i = 0; i < len; ++i) {
    value.push_back(kChars[dist(rng)]);
  }
  return value;
}

HostSessionRules NormalizeRules(HostSessionRules rules) {
  if (rules.defaultPort <= 0 || rules.defaultPort > 65535) rules.defaultPort = 9443;
  if (rules.defaultBindAddress.empty()) rules.defaultBindAddress = L"0.0.0.0";
  if (rules.generatedRoomLength == 0) rules.generatedRoomLength = 6;
  if (rules.generatedTokenLength == 0) rules.generatedTokenLength = 16;
  if (rules.roomRule.empty()) rules.roomRule = L"letters / digits, 3-32 chars";
  if (rules.tokenRule.empty()) rules.tokenRule = L"letters / digits, 6-64 chars";
  return rules;
}

HostSessionConfig NormalizeConfig(HostSessionConfig config, const HostSessionRules& rules) {
  if (config.bindAddress.empty()) config.bindAddress = rules.defaultBindAddress;
  if (config.port <= 0 || config.port > 65535) config.port = rules.defaultPort;
  return config;
}

bool ConfigEquals(const HostSessionConfig& lhs, const HostSessionConfig& rhs) {
  return lhs.bindAddress == rhs.bindAddress && lhs.port == rhs.port && lhs.room == rhs.room && lhs.token == rhs.token;
}

void ResetDeliveryState(HostSessionFlags& flags, bool* applied) {
  const bool changed = flags.viewerUrlCopied || flags.shareBundleExported || flags.shareWizardOpened ||
                       flags.handoffStarted || flags.handoffDelivered;
  flags.viewerUrlCopied = false;
  flags.shareBundleExported = false;
  flags.shareWizardOpened = false;
  flags.handoffStarted = false;
  flags.handoffDelivered = false;
  if (applied) *applied = changed;
}

HostSessionMutationResult BuildMutationResult(const HostSessionState& prior,
                                              HostSessionState next,
                                              bool generatedRoom,
                                              bool generatedToken,
                                              bool resetRequested) {
  HostSessionMutationResult result;
  result.state = std::move(next);
  result.generatedRoom = generatedRoom;
  result.generatedToken = generatedToken;
  result.configChanged = !ConfigEquals(NormalizeConfig(prior.config, NormalizeRules(prior.rules)),
                                       NormalizeConfig(result.state.config, NormalizeRules(result.state.rules)));
  if (resetRequested && (result.configChanged || generatedRoom || generatedToken)) {
    ResetDeliveryState(result.state.flags, &result.resetDeliveryStateApplied);
  }
  return result;
}

} // namespace

HostSessionState NormalizeHostSessionState(const HostSessionState& state) {
  HostSessionState normalized = state;
  normalized.rules = NormalizeRules(normalized.rules);
  normalized.config = NormalizeConfig(std::move(normalized.config), normalized.rules);
  return normalized;
}

HostSessionMutationResult ApplyHostSessionConfig(const HostSessionState& current,
                                                std::wstring room,
                                                std::wstring token,
                                                std::wstring bindAddress,
                                                int port) {
  HostSessionState next = NormalizeHostSessionState(current);
  next.config.room = std::move(room);
  next.config.token = std::move(token);
  next.config.bindAddress = std::move(bindAddress);
  next.config.port = port;
  next = NormalizeHostSessionState(next);
  return BuildMutationResult(current, std::move(next), false, false, true);
}

HostSessionMutationResult EnsureHostSessionCredentials(const HostSessionState& current) {
  HostSessionState next = NormalizeHostSessionState(current);
  bool generatedRoom = false;
  bool generatedToken = false;
  if (next.config.room.empty()) {
    next.config.room = MakeRandomAlnum(next.rules.generatedRoomLength);
    generatedRoom = true;
  }
  if (next.config.token.empty()) {
    next.config.token = MakeRandomAlnum(next.rules.generatedTokenLength);
    generatedToken = true;
  }
  return BuildMutationResult(current, std::move(next), generatedRoom, generatedToken, true);
}

HostSessionMutationResult GenerateHostSessionCredentials(const HostSessionState& current) {
  HostSessionState next = NormalizeHostSessionState(current);
  next.config.room = MakeRandomAlnum(next.rules.generatedRoomLength);
  next.config.token = MakeRandomAlnum(next.rules.generatedTokenLength);
  return BuildMutationResult(current, std::move(next), true, true, true);
}

HostSessionAdminModel BuildHostSessionAdminModel(const HostSessionState& state) {
  const HostSessionState normalized = NormalizeHostSessionState(state);

  HostSessionAdminModel model;
  model.bindAddress = normalized.config.bindAddress;
  model.port = normalized.config.port;
  model.room = normalized.config.room;
  model.token = normalized.config.token;
  model.defaultPort = normalized.rules.defaultPort;
  model.defaultBindAddress = normalized.rules.defaultBindAddress;
  model.roomRule = normalized.rules.roomRule;
  model.tokenRule = normalized.rules.tokenRule;
  model.viewerUrlCopied = normalized.flags.viewerUrlCopied;
  model.shareBundleExported = normalized.flags.shareBundleExported;
  model.shareWizardOpened = normalized.flags.shareWizardOpened;
  model.handoffStarted = normalized.flags.handoffStarted;
  model.handoffDelivered = normalized.flags.handoffDelivered;
  return model;
}

} // namespace lan::runtime
