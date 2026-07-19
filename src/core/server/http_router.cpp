#include "http_router.h"

#include "core/protocol/messages.h"
#include "ws_hub.h"

#include <cinttypes>
#include <cstdio>
#include <filesystem>
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

// Weak validator derived from file size + mtime. Cheap to compute, changes
// whenever the file is rebuilt/copied, and needs no date parsing.
inline std::string ComputeFileEtag(const std::string& path) {
  std::error_code ec;
  const std::filesystem::path p(path);
  const auto size = std::filesystem::file_size(p, ec);
  if (ec) return {};
  const auto mtime = std::filesystem::last_write_time(p, ec);
  if (ec) return {};
  const auto ticks = static_cast<std::uint64_t>(mtime.time_since_epoch().count());
  char buf[48];
  std::snprintf(buf, sizeof(buf), "\"%" PRIx64 "-%" PRIx64 "\"",
                static_cast<std::uint64_t>(size), ticks);
  return buf;
}

} // namespace

std::string HttpRouter::StripQuery(std::string_view target) {
  std::string t(target);
  auto pos = t.find('?');
  if (pos != std::string::npos) t.resize(pos);
  return t;
}

HttpRouter::HttpRouter(std::string wwwRoot, std::string adminRoot, std::shared_ptr<WsHub> hub)
    : wwwRoot_(std::move(wwwRoot)), adminRoot_(std::move(adminRoot)), hub_(std::move(hub)) {}

std::string HttpRouter::MapPath(std::string_view target) const {
  const auto t = StripQuery(target);
  if (t.find("..") != std::string::npos) return {};

  if (t == "/admin" || t == "/admin/" || t == "/admin/index.html") return adminRoot_ + "/index.html";
  if (t.rfind("/admin/", 0) == 0) return adminRoot_ + std::string(t.substr(std::string_view("/admin").size()));

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

  // Conditional GET: clients cache assets but must revalidate ("no-cache");
  // an unchanged file answers with an empty 304 instead of the full body.
  const auto etag = ComputeFileEtag(path);
  if (!etag.empty()) {
    const auto inm = req.find(http::field::if_none_match);
    if (inm != req.end() && inm->value() == etag) {
      auto res = MakeText(req, http::status::not_modified, GuessMime(path), std::string());
      res.set(http::field::etag, etag);
      res.set(http::field::cache_control, "no-cache");
      return res;
    }
  }

  std::string content;
  if (!ReadAllBytes(path, content)) return NotFound(req);

  auto res = MakeText(req, http::status::ok, GuessMime(path), std::move(content));
  if (!etag.empty()) {
    res.set(http::field::etag, etag);
    res.set(http::field::cache_control, "no-cache");
  }
  return res;
}

// Explicit instantiation
template http::response<http::string_body> HttpRouter::HandleRequest(
    const http::request<http::string_body, http::fields>& req);

} // namespace lan::server
