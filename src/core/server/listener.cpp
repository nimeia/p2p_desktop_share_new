#include "listener.h"
#include "http_session.h"
#include "tls_context.h"
#include "ws_hub.h"
#include "http_router.h"
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/error.hpp>

namespace lan::server {
using tcp = boost::asio::ip::tcp;

Listener::Listener(boost::asio::io_context& ioc,
                   std::shared_ptr<TlsContext> tls,
                   std::shared_ptr<WsHub> hub,
                   std::shared_ptr<HttpRouter> router,
                   tcp::endpoint ep)
  : ioc_(ioc),
    tls_(std::move(tls)),
    hub_(std::move(hub)),
    router_(std::move(router)),
    acceptor_(ioc) {
  acceptor_.open(ep.protocol());
  acceptor_.set_option(tcp::acceptor::reuse_address(true));
  acceptor_.bind(ep);
  acceptor_.listen();
}

void Listener::Run() { DoAccept(); }

void Listener::Stop() {
  boost::system::error_code ec;
  acceptor_.close(ec);
}

void Listener::DoAccept() {
  acceptor_.async_accept(
    boost::asio::make_strand(ioc_),
    [this](boost::system::error_code ec, tcp::socket socket) {
      if (!ec) {
        // Spawn a session per connection
        std::make_shared<HttpSession>(std::move(socket), tls_->Ctx(), hub_, router_)->Run();
      } else {
        if (ec == boost::asio::error::operation_aborted) return;
      }
      if (acceptor_.is_open()) DoAccept();
    }
  );
}

} // namespace lan::server
