#include "core/server/service_host.h"
#include "core/cert/cert_manager.h"
#include "core/network/network_manager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

static std::string argValue(int argc, char** argv, const std::string& key, const std::string& def) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (key == argv[i]) return argv[i + 1];
  }
  return def;
}

static bool hasArg(int argc, char** argv, const std::string& key) {
  for (int i = 1; i < argc; ++i) {
    if (key == argv[i]) return true;
  }
  return false;
}

static int argInt(int argc, char** argv, const std::string& key, int def) {
  try {
    return std::stoi(argValue(argc, argv, key, std::to_string(def)));
  } catch (...) {
    return def;
  }
}

static fs::path executableDir(const char* argv0) {
  try {
    return fs::weakly_canonical(fs::path(argv0)).parent_path();
  } catch (...) {
    return fs::current_path();
  }
}

static std::string resolveRuntimePath(const fs::path& baseDir, const std::string& value) {
  fs::path path(value);
  if (path.is_absolute()) return path.string();
  return (baseDir / path).lexically_normal().string();
}

int main(int argc, char** argv) {
  const fs::path baseDir = executableDir(argv[0]);
  std::string bind = argValue(argc, argv, "--bind", "0.0.0.0");
  int port = argInt(argc, argv, "--port", 9443);
  std::string www = resolveRuntimePath(baseDir, argValue(argc, argv, "--www", "www"));
  std::string certdir = resolveRuntimePath(baseDir, argValue(argc, argv, "--certdir", "cert"));
  std::string sanIp = argValue(argc, argv, "--san-ip", "127.0.0.1");

  // Behavior:
  // - default: wait for ENTER on stdin (classic console behavior)
  // - --no-stdin: run indefinitely (GUI-spawn friendly)
  // - --wait-stdin: force wait even if other flags are present
  // - --run-for <seconds>: run for N seconds then stop (useful for smoke tests)
  bool noStdin = hasArg(argc, argv, "--no-stdin");
  bool waitStdin = hasArg(argc, argv, "--wait-stdin");
  if (!waitStdin) waitStdin = !noStdin;

  int runForSec = argInt(argc, argv, "--run-for", 0);

  lan::cert::CertPaths cp{};
  std::string err;
  if (!lan::cert::CertManager::EnsureSelfSigned(certdir, sanIp, cp, err)) {
    std::cerr << "Cert error: " << err << "\n";
    return 2;
  }

  lan::server::ServiceHost host;
  lan::server::ServiceConfig cfg;
  cfg.bindAddress = bind;
  cfg.port = static_cast<uint16_t>(port);
  cfg.wwwRoot = www;
  cfg.certFile = cp.certFile;
  cfg.keyFile = cp.keyFile;

  if (!host.Start(cfg)) {
    std::cerr << "Failed to start service\n";
    return 3;
  }

  std::cout << "Service running on https://" << bind << ":" << port << "\n";
  std::cout << "WS endpoint: wss://<host>:" << port << "/ws\n";

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
    // stdin isn't available (e.g., GUI spawn without console). Fall back to run-forever mode.
    std::cout << "stdin unavailable (EOF). Running until terminated...\n";
  } else {
    std::cout << "Running without stdin; terminate the process to stop.\n";
  }

  // Run forever (until terminated by the host app / OS).
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
