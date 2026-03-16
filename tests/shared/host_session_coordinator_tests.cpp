#include "core/runtime/host_session_coordinator.h"

#include <cstdlib>
#include <iostream>

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "host session coordinator test failed: " << message << "\n";
    std::exit(1);
  }
}

} // namespace

int main() {
  using namespace lan::runtime;

  HostSessionState state;
  state.rules.defaultPort = 9443;
  state.rules.defaultBindAddress = L"0.0.0.0";
  state.rules.generatedRoomLength = 6;
  state.rules.generatedTokenLength = 16;
  state.config.port = 0;
  state.flags.viewerUrlCopied = true;
  state.flags.shareBundleExported = true;
  state.flags.handoffDelivered = true;

  auto ensured = EnsureHostSessionCredentials(state);
  Expect(ensured.state.config.bindAddress == L"0.0.0.0", "ensure should apply default bind");
  Expect(ensured.state.config.port == 9443, "ensure should apply default port");
  Expect(ensured.generatedRoom, "ensure should generate room");
  Expect(ensured.generatedToken, "ensure should generate token");
  Expect(ensured.state.config.room.size() == 6, "generated room length should match rules");
  Expect(ensured.state.config.token.size() == 16, "generated token length should match rules");
  Expect(!ensured.state.flags.viewerUrlCopied, "ensure should reset viewer copied flag");
  Expect(!ensured.state.flags.handoffDelivered, "ensure should reset delivered state");

  HostSessionState existing = ensured.state;
  existing.flags.viewerUrlCopied = true;
  auto applied = ApplyHostSessionConfig(existing, L"meeting-room", L"persistent-token", L"192.168.1.10", 10443);
  Expect(applied.configChanged, "apply should mark config changed");
  Expect(applied.state.config.room == L"meeting-room", "apply should update room");
  Expect(applied.state.config.token == L"persistent-token", "apply should update token");
  Expect(applied.state.config.bindAddress == L"192.168.1.10", "apply should update bind address");
  Expect(applied.state.config.port == 10443, "apply should update port");
  Expect(!applied.state.flags.viewerUrlCopied, "apply should reset handoff flags");

  auto generated = GenerateHostSessionCredentials(applied.state);
  Expect(generated.generatedRoom && generated.generatedToken, "generate should regenerate both credentials");
  Expect(generated.state.config.room != applied.state.config.room, "generate should replace room");
  Expect(generated.state.config.token != applied.state.config.token, "generate should replace token");

  const auto admin = BuildHostSessionAdminModel(generated.state);
  Expect(admin.port == generated.state.config.port, "admin model should copy current port");
  Expect(admin.defaultBindAddress == L"0.0.0.0", "admin model should expose default bind");
  Expect(!admin.shareBundleExported, "admin model should reflect reset flags");

  std::cout << "host session coordinator tests passed\n";
  return 0;
}
