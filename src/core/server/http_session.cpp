#include "http_session.h"
#include "http_router.h"
#include "ws_session.h"
#include "ws_hub.h"
#include <boost/beast/websocket.hpp>

namespace lan::server {
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;

HttpSession::HttpSession(boost::asio::ip::tcp::socket socket,
                         boost::asio::ssl::context& sslCtx,
                         std::shared_ptr<WsHub> hub,
                         std::shared_ptr<HttpRouter> router)
  : stream_(std::move(socket), sslCtx), hub_(std::move(hub)), router_(std::move(router)) {}

void HttpSession::Run() {
  // TLS handshake
  stream_.async_handshake(boost::asio::ssl::stream_base::server,
    [self = shared_from_this()](boost::system::error_code ec) { self->OnHandshake(ec); });
}

void HttpSession::OnHandshake(boost::system::error_code ec) {
  if (ec) return;
  DoRead();
}

void HttpSession::DoRead() {
  req_ = {};
  http::async_read(stream_, buffer_, req_,
    [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) { self->OnRead(ec, bytes); });
}

void HttpSession::OnRead(boost::system::error_code ec, std::size_t) {
  if (ec == http::error::end_of_stream ||
      ec == boost::asio::ssl::error::stream_truncated ||
      ec == boost::asio::error::eof) {
    DoClose();
    return;
  }
  if (ec) return;

  // WS upgrade?
  if (websocket::is_upgrade(req_) && req_.target() == "/ws") {
    websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> ws{std::move(stream_)};
    std::make_shared<WsSession>(std::move(ws), hub_)->Run(std::move(req_));
    return;
  }

  HandleRequest();
}

void HttpSession::HandleRequest() {
  res_ = std::make_shared<http::response<http::string_body>>(router_->HandleRequest(req_));
  const bool close = res_->need_eof();
  http::async_write(stream_, *res_,
    [self = shared_from_this(), close](boost::system::error_code ec, std::size_t bytes) {
      self->OnWrite(close, ec, bytes);
    });
}

void HttpSession::OnWrite(bool close, boost::system::error_code ec, std::size_t) {
  if (ec) return;

  res_.reset();

  if (close) {
    DoClose();
    return;
  }

  DoRead();
}

void HttpSession::DoClose() {
  stream_.async_shutdown(
    [self = shared_from_this()](boost::system::error_code ec) { self->OnShutdown(ec); });
}

void HttpSession::OnShutdown(boost::system::error_code ec) {
  if (ec == boost::asio::ssl::error::stream_truncated ||
      ec == boost::asio::error::eof) {
    ec = {};
  }
}

} // namespace lan::server
