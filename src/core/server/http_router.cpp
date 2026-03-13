#include "http_router.h"

#include "core/protocol/messages.h"
#include "ws_hub.h"

#include <fstream>
#include <sstream>

namespace lan::server {
namespace http = boost::beast::http;

namespace {

template <class Body, class Allocator>
http::response<http::string_body> MakeText(
    const http::request<Body, http::basic_fields<Allocator>>& req,
    http::status st,
    std::string_view contentType,
    std::string body) {
  http::response<http::string_body> res;
  res.version(req.version());
  res.set(http::field::server, "lan-screenshare");
  res.keep_alive(req.keep_alive());

  res.result(st);
  res.set(http::field::content_type, contentType);
  res.body() = std::move(body);
  res.prepare_payload();
  return res;
}

template <class Body, class Allocator>
http::response<http::string_body> NotFound(const http::request<Body, http::basic_fields<Allocator>>& req) {
  return MakeText(req, http::status::not_found, "text/plain; charset=utf-8", "Not Found");
}

inline bool ReadAllBytes(const std::string& path, std::string& out) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return false;
  std::ostringstream oss;
  oss << ifs.rdbuf();
  out = oss.str();
  return true;
}

} // namespace

std::string HttpRouter::StripQuery(std::string_view target) {
  std::string t(target);
  auto pos = t.find('?');
  if (pos != std::string::npos) t.resize(pos);
  return t;
}

HttpRouter::HttpRouter(std::string wwwRoot, std::shared_ptr<WsHub> hub)
    : wwwRoot_(std::move(wwwRoot)), hub_(std::move(hub)) {}

std::string HttpRouter::MapPath(std::string_view target) const {
  const auto t = StripQuery(target);
  if (t.find("..") != std::string::npos) return {};

  // Minimal routing
  if (t.rfind("/host", 0) == 0) return wwwRoot_ + "/host.html";
  if (t.rfind("/view", 0) == 0) return wwwRoot_ + "/viewer.html";
  if (t == "/host-app.webmanifest") return wwwRoot_ + "/host-app.webmanifest";
  if (t == "/host-sw.js") return wwwRoot_ + "/host-sw.js";
  if (t == "/viewer-app.webmanifest") return wwwRoot_ + "/viewer-app.webmanifest";
  if (t == "/viewer-sw.js") return wwwRoot_ + "/viewer-sw.js";
  if (t.rfind("/assets/", 0) == 0) return wwwRoot_ + t;
  return {};
}

std::string HttpRouter::GuessMime(std::string_view path) {
  if (path.ends_with(".html")) return "text/html; charset=utf-8";
  if (path.ends_with(".js")) return "application/javascript; charset=utf-8";
  if (path.ends_with(".webmanifest")) return "application/manifest+json; charset=utf-8";
  if (path.ends_with(".json")) return "application/json; charset=utf-8";
  if (path.ends_with(".css")) return "text/css; charset=utf-8";
  if (path.ends_with(".svg")) return "image/svg+xml";
  return "application/octet-stream";
}

template <class Body, class Allocator>
http::response<http::string_body> HttpRouter::HandleRequest(
    const http::request<Body, http::basic_fields<Allocator>>& req) {
  if (req.method() != http::verb::get) {
    return MakeText(req, http::status::method_not_allowed, "text/plain; charset=utf-8", "Method Not Allowed");
  }

  const auto target = StripQuery(req.target());

  if (target == "/health") {
    return MakeText(req, http::status::ok, "text/plain; charset=utf-8", "ok");
  }

  if (target == "/api/status") {
    std::size_t rooms = 0;
    std::size_t viewers = 0;
    if (hub_) {
      auto st = hub_->GetStats();
      rooms = st.rooms;
      viewers = st.viewers;
    }

    auto body = lan::protocol::ApiStatus(true, rooms, viewers);
    return MakeText(req, http::status::ok, "application/json; charset=utf-8", std::move(body));
  }

  const auto path = MapPath(target);
  if (path.empty()) return NotFound(req);

  std::string content;
  if (!ReadAllBytes(path, content)) return NotFound(req);

  auto res = MakeText(req, http::status::ok, GuessMime(path), std::move(content));
  return res;
}

// Explicit instantiation
template http::response<http::string_body> HttpRouter::HandleRequest(
    const http::request<http::string_body, http::fields>& req);

} // namespace lan::server
