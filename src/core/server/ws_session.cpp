#include "ws_session.h"

#include "ws_hub.h"

#include <boost/asio/bind_executor.hpp>
#include <boost/beast/core/buffers_to_string.hpp>

namespace lan::server {
namespace websocket = boost::beast::websocket;

WsSession::WsSession(stream_t ws, std::shared_ptr<WsHub> hub)
    : ws_(std::move(ws)), strand_(boost::asio::make_strand(ws_.get_executor())), hub_(std::move(hub)) {}

void WsSession::Run() {
  ws_.set_option(websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
  ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
    res.set(boost::beast::http::field::server, "lan-screenshare");
  }));

  ws_.async_accept(boost::asio::bind_executor(
      strand_, [self = shared_from_this()](boost::system::error_code ec) { self->OnAccept(ec); }));
}

void WsSession::Run(boost::beast::http::request<boost::beast::http::string_body> req) {
  ws_.set_option(websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
  ws_.set_option(websocket::stream_base::decorator([](websocket::response_type& res) {
    res.set(boost::beast::http::field::server, "lan-screenshare");
  }));

  ws_.async_accept(
      req,
      boost::asio::bind_executor(
          strand_, [self = shared_from_this()](boost::system::error_code ec) { self->OnAccept(ec); }));
}

void WsSession::OnAccept(boost::system::error_code ec) {
  if (ec) return;
  DoRead();
}

void WsSession::DoRead() {
  ws_.async_read(buffer_, boost::asio::bind_executor(
                           strand_,
                           [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) {
                             self->OnRead(ec, bytes);
                           }));
}

void WsSession::OnRead(boost::system::error_code ec, std::size_t) {
  if (ec) {
    if (hub_ && !room_.empty() && !peerId_.empty()) {
      hub_->Leave(room_, peerId_);
    }
    return;
  }

  const auto text = boost::beast::buffers_to_string(buffer_.data());
  buffer_.consume(buffer_.size());

  HandleMessage(text);
  DoRead();
}

void WsSession::Send(std::string text) {
  boost::asio::post(strand_, [self = shared_from_this(), text = std::move(text)]() mutable {
    const bool idle = self->outbox_.empty();
    self->outbox_.push_back(std::move(text));

    if (idle && !self->writing_) {
      self->writing_ = true;
      self->DoWrite();
    }
  });
}

void WsSession::DoWrite() {
  ws_.text(true);
  ws_.async_write(
      boost::asio::buffer(outbox_.front()),
      boost::asio::bind_executor(
          strand_,
          [self = shared_from_this()](boost::system::error_code ec, std::size_t bytes) { self->OnWrite(ec, bytes); }));
}

void WsSession::OnWrite(boost::system::error_code ec, std::size_t) {
  if (ec) {
    writing_ = false;
    outbox_.clear();
    return;
  }

  outbox_.pop_front();
  if (!outbox_.empty()) {
    DoWrite();
    return;
  }

  writing_ = false;
}

} // namespace lan::server
