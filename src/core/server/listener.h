#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <memory>

namespace lan::server {

class WsHub;
class HttpRouter;

class Listener {
public:
  Listener(boost::asio::io_context& ioc,
           std::shared_ptr<WsHub> hub,
           std::shared_ptr<HttpRouter> router,
           boost::asio::ip::tcp::endpoint ep);

  void Run();
  void Stop();

private:
  void DoAccept();

  boost::asio::io_context& ioc_;
  std::shared_ptr<WsHub> hub_;
  std::shared_ptr<HttpRouter> router_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

} // namespace lan::server
