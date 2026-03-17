#include "http_session.h"
#include "http_router.h"
#include "ws_session.h"
#include "ws_hub.h"
#include <boost/beast/websocket.hpp>

namespace lan::server {
namespace http = boost::beast::http;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

HttpSession::HttpSession(boost::asio::ip::tcp::socket socket,
                         std::shared_ptr<WsHub> hub,
                         std::shared_ptr<HttpRouter> router)
  : stream_(std::move(socket)), hub_(std::move(hub)), router_(std::move(router)) {}

void HttpSession::Run() {
  DoRead();
}

void HttpSession::DoRead() {
  req_ = {};
  http::async_read(stream_, buffer_, req_,
    [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) { self->OnRead(ec, bytes); });
}

void HttpSession::OnRead(boost::system::error_code ec, std::size_t) {
  if (ec == http::error::end_of_stream || ec == boost::asio::error::eof) {
    DoClose();
    return;
  }
  if (ec) return;

  // WS upgrade?
  if (websocket::is_upgrade(req_) && req_.target() == "/ws") {
    websocket::stream<boost::beast::tcp_stream> ws{std::move(stream_)};
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
  boost::system::error_code ec;
  boost::beast::get_lowest_layer(stream_).socket().shutdown(tcp::socket::shutdown_send, ec);
}

} // namespace lan::server
