#include "service_host.h"

#include "http_router.h"
#include "listener.h"
#include "tls_context.h"
#include "ws_hub.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <algorithm>
#include <thread>
#include <vector>

namespace lan::server {

struct ServiceHost::Impl {
  ServiceConfig cfg{};
  ServiceStatus status{};

  boost::asio::io_context ioc{1};
  std::shared_ptr<TlsContext> tls;
  std::shared_ptr<WsHub> hub;
  std::unique_ptr<Listener> listener;
  std::vector<std::thread> threads;

  void RunThreads(std::size_t n) {
    threads.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
      threads.emplace_back([this] { ioc.run(); });
    }
  }
};

ServiceHost::ServiceHost() : impl_(new Impl) {}
ServiceHost::~ServiceHost() { Stop(); }

bool ServiceHost::Start(const ServiceConfig& cfg) {
  if (impl_->status.running) return false;

  // Allow restarting after a prior Stop().
  impl_->ioc.restart();

  impl_->cfg = cfg;

  boost::system::error_code aec;
  const auto addr = boost::asio::ip::make_address(cfg.bindAddress, aec);
  if (aec) return false;

  const std::size_t autoThreads = std::max<std::size_t>(2, std::thread::hardware_concurrency() / 2);
  const std::size_t threadCount = (cfg.threadCount == 0 ? autoThreads : cfg.threadCount);

  try {
    impl_->tls = std::make_shared<TlsContext>(cfg.certFile, cfg.keyFile);
    impl_->hub = std::make_shared<WsHub>(cfg.maxViewers);

    auto router = std::make_shared<HttpRouter>(cfg.wwwRoot, impl_->hub);
    const auto ep = boost::asio::ip::tcp::endpoint{addr, cfg.port};

    impl_->listener = std::make_unique<Listener>(impl_->ioc, impl_->tls, impl_->hub, router, ep);
    impl_->listener->Run();

    impl_->RunThreads(threadCount);
  } catch (...) {
    Stop();
    return false;
  }

  impl_->status.running = true;
  impl_->status.bindAddress = cfg.bindAddress;
  impl_->status.port = cfg.port;
  return true;
}

void ServiceHost::Stop() {
  if (!impl_->status.running) return;

  if (impl_->listener) impl_->listener->Stop();
  impl_->ioc.stop();

  for (auto& t : impl_->threads) {
    if (t.joinable()) t.join();
  }
  impl_->threads.clear();

  impl_->listener.reset();
  impl_->hub.reset();
  impl_->tls.reset();

  // Ready for next Start().
  impl_->ioc.restart();
  impl_->status = {};
}

ServiceStatus ServiceHost::GetStatus() const {
  auto st = impl_->status;
  if (impl_->hub) {
    const auto hs = impl_->hub->GetStats();
    st.rooms = hs.rooms;
    st.viewers = hs.viewers;
  }
  return st;
}

std::string ServiceHost::BuildHostUrl(const std::string& hostIp,
                                     const std::string& roomId,
                                     const std::string& hostToken) const {
  return "https://" + hostIp + ":" + std::to_string(impl_->cfg.port) +
         "/host?room=" + roomId + "&token=" + hostToken;
}

std::string ServiceHost::BuildViewerUrl(const std::string& hostIp, const std::string& roomId) const {
  return "https://" + hostIp + ":" + std::to_string(impl_->cfg.port) + "/view?room=" + roomId;
}

} // namespace lan::server
