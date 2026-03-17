#pragma once
#include <string>
#include <memory>
#include <cstdint>

namespace lan::server {

struct ServiceConfig {
  std::string bindAddress;  // e.g. "0.0.0.0"
  uint16_t port = 9443;
  std::string wwwRoot;      // absolute or relative path to www/
  std::string adminRoot;    // absolute or relative path to admin webui/
  size_t maxViewers = 10;
  size_t threadCount = 0;  // 0=auto
};

struct ServiceStatus {
  bool running = false;
  std::string bindAddress;
  uint16_t port = 0;
  size_t rooms = 0;
  size_t viewers = 0;
};

class ServiceHost {
public:
  ServiceHost();
  ~ServiceHost();

  // Start HTTP + WS (non-blocking). Returns false if already running or failed.
  bool Start(const ServiceConfig& cfg);

  // Stop service, joins threads.
  void Stop();

  ServiceStatus GetStatus() const;

  // Helpers for URL composition
  std::string BuildHostUrl(const std::string& hostIp, const std::string& roomId, const std::string& hostToken) const;
  std::string BuildViewerUrl(const std::string& hostIp, const std::string& roomId) const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace lan::server
