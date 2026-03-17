#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <memory>

namespace lan::server {

class WsHub;
class HttpRouter;

class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
  HttpSession(boost::asio::ip::tcp::socket socket,
              std::shared_ptr<WsHub> hub,
              std::shared_ptr<HttpRouter> router);

  void Run();

private:
  void DoRead();
  void OnRead(boost::system::error_code ec, std::size_t bytes);
  void HandleRequest();
  void OnWrite(bool close, boost::system::error_code ec, std::size_t bytes);
  void DoClose();

  boost::beast::tcp_stream stream_;
  boost::beast::flat_buffer buffer_;

  std::shared_ptr<WsHub> hub_;
  std::shared_ptr<HttpRouter> router_;

  boost::beast::http::request<boost::beast::http::string_body> req_;
  std::shared_ptr<boost::beast::http::response<boost::beast::http::string_body>> res_;
};

} // namespace lan::server
