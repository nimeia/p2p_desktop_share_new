#pragma once
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/json.hpp>
#include <memory>
#include <string>
#include <deque>

namespace lan::server {

class WsHub;

class WsSession : public std::enable_shared_from_this<WsSession> {
public:
  using stream_t = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;

  WsSession(stream_t ws, std::shared_ptr<WsHub> hub);

  void Run();
  void Run(boost::beast::http::request<boost::beast::http::string_body> req);
  void Send(std::string text);

  const std::string& Room() const { return room_; }
  const std::string& PeerId() const { return peerId_; }

private:
  void OnAccept(boost::system::error_code ec);
  void DoRead();
  void OnRead(boost::system::error_code ec, std::size_t bytes);

  void HandleMessage(const std::string& text);

  // Message handlers (see ws_session_handlers.cpp)
  void HandleHostRegister(const boost::json::object& obj);
  void HandleRoomJoin(const boost::json::object& obj);
  void HandleSessionEnd(const boost::json::object& obj);
  void HandleWebrtcForward(boost::json::object obj);

  bool ValidateHostToken(const boost::json::object& obj) const;

  void DoWrite();
  void OnWrite(boost::system::error_code ec, std::size_t bytes);

  stream_t ws_;
  // Use polymorphic executor that wraps a strand. This avoids template instantiation
  // issues with strand<any_io_executor> on some Boost/MSVC combinations.
  boost::asio::any_io_executor strand_;
  std::deque<std::string> outbox_;
  bool writing_ = false;
  boost::beast::flat_buffer buffer_;
  std::shared_ptr<WsHub> hub_;

  std::string room_;
  std::string peerId_; // "host" or "v-xxx"
  std::string hostToken_;
  bool isHost_ = false;
};

} // namespace lan::server
