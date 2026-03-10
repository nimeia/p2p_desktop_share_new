#include "tls_context.h"
#include <boost/asio/ssl.hpp>

namespace lan::server {

TlsContext::TlsContext(const std::string& certFile, const std::string& keyFile)
  : ctx_(boost::asio::ssl::context::tls_server) {
  ctx_.set_options(
      boost::asio::ssl::context::default_workarounds |
      boost::asio::ssl::context::no_sslv2 |
      boost::asio::ssl::context::no_sslv3 |
      boost::asio::ssl::context::single_dh_use);

  ctx_.use_certificate_chain_file(certFile);
  ctx_.use_private_key_file(keyFile, boost::asio::ssl::context::file_format::pem);

  // TODO: set verify options if needed
}

boost::asio::ssl::context& TlsContext::Ctx() { return ctx_; }

} // namespace lan::server
