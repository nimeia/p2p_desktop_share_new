#include "core/server/service_host.h"
#include "platform/abstraction/cert_provider.h"
#include "platform/abstraction/factory.h"
#include "platform/abstraction/network_service.h"
#include "platform/abstraction/runtime_paths.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

std::string ArgValue(int argc, char** argv, const std::string& key, const std::string& def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (key == argv[i]) return argv[i + 1];
  }
  return def;
}

bool HasArg(int argc, char** argv, const std::string& key) {
  for (int i = 1; i < argc; ++i) {
    if (key == argv[i]) return true;
  }
  return false;
}

int ArgInt(int argc, char** argv, const std::string& key, int def) {
  try {
    return std::stoi(ArgValue(argc, argv, key, std::to_string(def)));
  } catch (...) {
    return def;
  }
}

} // namespace

int main(int argc, char** argv) {
  const fs::path baseDir = lan::platform::ExecutableDir(argv[0]);

  lan::platform::ServerEndpointRequest endpointRequest;
  endpointRequest.bindAddress = ArgValue(argc, argv, "--bind", "0.0.0.0");
  endpointRequest.subjectAltNames = ArgValue(argc, argv, "--san-ip", lan::platform::kAutoAddressToken);

  const int port = ArgInt(argc, argv, "--port", 9443);
  const std::string www = lan::platform::ResolveRuntimePath(baseDir, ArgValue(argc, argv, "--www", "www"));
  const std::string certdir = lan::platform::ResolveRuntimePath(baseDir, ArgValue(argc, argv, "--certdir", "cert"));

  bool noStdin = HasArg(argc, argv, "--no-stdin");
  bool waitStdin = HasArg(argc, argv, "--wait-stdin");
  if (!waitStdin) waitStdin = !noStdin;

  const int runForSec = ArgInt(argc, argv, "--run-for", 0);

  auto certProvider = lan::platform::CreateDefaultCertProvider();
  auto networkService = lan::platform::CreateDefaultNetworkService();

  lan::platform::ServerEndpointResolution endpointResolution;
  std::string endpointErr;
  if (!networkService->ResolveServerEndpoints(endpointRequest, endpointResolution, endpointErr)) {
    std::cerr << "Network resolution error: " << endpointErr << "\n";
    return 2;
  }

  lan::cert::CertPaths certPaths{};
  lan::platform::ServerCertificateRequest certRequest;
  certRequest.outputDirectory = certdir;
  certRequest.subjectAltNames = endpointResolution.subjectAltNames;

  std::string certErr;
  if (!certProvider->EnsureServerCertificate(certRequest, certPaths, certErr)) {
    std::cerr << "Cert error: " << certErr << "\n";
    return 3;
  }

  lan::server::ServiceHost host;
  lan::server::ServiceConfig cfg;
  cfg.bindAddress = endpointResolution.bindAddress;
  cfg.port = static_cast<std::uint16_t>(port);
  cfg.wwwRoot = www;
  cfg.certFile = certPaths.certFile;
  cfg.keyFile = certPaths.keyFile;

  if (!host.Start(cfg)) {
    std::cerr << "Failed to start service\n";
    return 4;
  }

  std::cout << "Service running on https://" << cfg.bindAddress << ':' << port << "\n";
  std::cout << "Preferred host for clients: " << endpointResolution.preferredHost << "\n";
  std::cout << "WS endpoint: wss://<host>:" << port << "/ws\n";
  std::cout << "Network provider: " << networkService->ProviderName() << "\n";
  std::cout << "Cert provider: " << certProvider->ProviderName() << "\n";
  if (!endpointResolution.detail.empty()) {
    std::cout << endpointResolution.detail << "\n";
  }

  if (runForSec > 0) {
    std::cout << "Running for " << runForSec << " seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(runForSec));
    host.Stop();
    return 0;
  }

  if (waitStdin) {
    std::cout << "Press ENTER to stop...\n";
    std::string line;
    if (std::getline(std::cin, line)) {
      host.Stop();
      return 0;
    }
    std::cout << "stdin unavailable (EOF). Running until terminated...\n";
  } else {
    std::cout << "Running without stdin; terminate the process to stop.\n";
  }

  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
