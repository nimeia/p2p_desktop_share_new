#pragma once
#include <boost/asio/ssl/context.hpp>
#include <string>

namespace lan::server {

class TlsContext {
public:
  TlsContext(const std::string& certFile, const std::string& keyFile);
  boost::asio::ssl::context& Ctx();

private:
  boost::asio::ssl::context ctx_;
};

} // namespace lan::server
