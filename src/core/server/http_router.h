#pragma once
#include <string>
#include <memory>
#include <boost/beast/http.hpp>

namespace lan::server {

class WsHub;

class HttpRouter {
public:
  HttpRouter(std::string wwwRoot, std::shared_ptr<WsHub> hub);

  template<class Body, class Allocator>
  boost::beast::http::response<boost::beast::http::string_body>
  HandleRequest(const boost::beast::http::request<Body, boost::beast::http::basic_fields<Allocator>>& req);

private:
  std::string wwwRoot_;
  std::shared_ptr<WsHub> hub_;

  static std::string StripQuery(std::string_view target);


  std::string MapPath(std::string_view target) const;
  static std::string GuessMime(std::string_view path);
};

} // namespace lan::server
