#include "core/server/service_host.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {
namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

#ifndef LAN_TEST_SOURCE_DIR
#define LAN_TEST_SOURCE_DIR "."
#endif

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "browser smoke test failed: " << message << "\n";
    std::exit(1);
  }
}

std::filesystem::path SourceDir() {
  return std::filesystem::path(LAN_TEST_SOURCE_DIR);
}

uint16_t PickFreePort() {
  net::io_context ioc;
  tcp::acceptor acceptor(ioc);
  boost::system::error_code ec;
  acceptor.open(tcp::v4(), ec);
  Expect(!ec, "acceptor open should succeed");
  acceptor.bind({net::ip::make_address("127.0.0.1"), 0}, ec);
  Expect(!ec, "acceptor bind should succeed");
  const auto port = acceptor.local_endpoint(ec).port();
  Expect(!ec && port != 0, "ephemeral port selection should succeed");
  acceptor.close(ec);
  return port;
}

struct HttpResult {
  int status = 0;
  std::string body;
  std::string contentType;
};

HttpResult HttpsGet(uint16_t port, const std::string& target) {
  net::io_context ioc;
  ssl::context ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);

  tcp::resolver resolver(ioc);
  beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
  beast::get_lowest_layer(stream).expires_after(std::chrono::seconds(5));
  auto const results = resolver.resolve("127.0.0.1", std::to_string(port));
  beast::get_lowest_layer(stream).connect(results);
  stream.handshake(ssl::stream_base::client);

  http::request<http::empty_body> req{http::verb::get, target, 11};
  req.set(http::field::host, "127.0.0.1");
  req.set(http::field::user_agent, "lan-browser-smoke");
  http::write(stream, req);

  beast::flat_buffer buffer;
  http::response<http::string_body> res;
  http::read(stream, buffer, res);

  boost::system::error_code ec;
  stream.shutdown(ec);
  if (ec == net::error::eof || ec == ssl::error::stream_truncated) {
    ec = {};
  }
  Expect(!ec, "https shutdown should not fail");

  HttpResult result;
  result.status = static_cast<int>(res.result_int());
  result.body = res.body();
  if (res.find(http::field::content_type) != res.end()) {
    const auto ct = res.at(http::field::content_type);
    result.contentType.assign(ct.data(), ct.size());
  }
  return result;
}

bool WaitForHealth(uint16_t port) {
  for (int attempt = 0; attempt < 60; ++attempt) {
    try {
      const auto res = HttpsGet(port, "/health");
      if (res.status == 200 && res.body == "ok") {
        return true;
      }
    } catch (...) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return false;
}

std::string JsonString(const boost::json::object& obj, const char* key) {
  const auto it = obj.find(key);
  if (it == obj.end() || !it->value().is_string()) return {};
  return std::string(it->value().as_string().c_str());
}

boost::json::object ParseJsonObject(const std::string& text) {
  boost::system::error_code ec;
  auto value = boost::json::parse(text, ec);
  Expect(!ec && value.is_object(), "expected JSON object");
  return value.as_object();
}

class WsClient {
public:
  explicit WsClient(uint16_t port)
      : ctx_(ssl::context::tlsv12_client), resolver_(ioc_), ws_(ioc_, ctx_) {
    ctx_.set_verify_mode(ssl::verify_none);
    auto const results = resolver_.resolve("127.0.0.1", std::to_string(port));
    beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(5));
    beast::get_lowest_layer(ws_).connect(results);
    ws_.next_layer().handshake(ssl::stream_base::client);
    ws_.handshake("127.0.0.1", "/ws");
  }

  void Send(const std::string& text) {
    ws_.write(net::buffer(text));
  }

  boost::json::object ReadObject() {
    beast::flat_buffer buffer;
    ws_.read(buffer);
    return ParseJsonObject(beast::buffers_to_string(buffer.data()));
  }

  void Close() {
    if (closed_) return;
    boost::system::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    closed_ = true;
    if (ec == net::error::eof || ec == ssl::error::stream_truncated || ec == websocket::error::closed) {
      ec = {};
    }
    Expect(!ec, "websocket close should not fail");
  }

  ~WsClient() {
    try {
      Close();
    } catch (...) {
    }
  }

private:
  net::io_context ioc_;
  ssl::context ctx_;
  tcp::resolver resolver_;
  websocket::stream<beast::ssl_stream<beast::tcp_stream>> ws_;
  bool closed_ = false;
};

} // namespace

int main() {
  using namespace lan::server;

  const auto sourceDir = SourceDir();
  const auto certFile = (sourceDir / "tests" / "fixtures" / "server_test_cert.pem").string();
  const auto keyFile = (sourceDir / "tests" / "fixtures" / "server_test_key.pem").string();
  const auto wwwRoot = (sourceDir / "www").string();
  const auto port = PickFreePort();

  ServiceHost host;
  ServiceConfig cfg;
  cfg.bindAddress = "127.0.0.1";
  cfg.port = port;
  cfg.wwwRoot = wwwRoot;
  cfg.certFile = certFile;
  cfg.keyFile = keyFile;
  cfg.threadCount = 2;
  cfg.maxViewers = 4;

  Expect(host.Start(cfg), "service host should start");
  Expect(WaitForHealth(port), "health endpoint should become ready");

  const auto health = HttpsGet(port, "/health");
  Expect(health.status == 200, "/health should return 200");
  Expect(health.body == "ok", "/health should return ok");

  const auto hostPage = HttpsGet(port, "/host?room=smoke-room&token=host-token");
  Expect(hostPage.status == 200, "host page should return 200");
  Expect(hostPage.body.find("LAN Screen Share - Host") != std::string::npos, "host page title should be present");
  Expect(hostPage.body.find("/assets/app_host.js") != std::string::npos, "host page should reference app_host.js");

  const auto viewerPage = HttpsGet(port, "/view?room=smoke-room");
  Expect(viewerPage.status == 200, "viewer page should return 200");
  Expect(viewerPage.body.find("LAN Screen Share - Viewer") != std::string::npos, "viewer page title should be present");
  Expect(viewerPage.body.find("/assets/app_viewer.js") != std::string::npos, "viewer page should reference app_viewer.js");

  const auto commonJs = HttpsGet(port, "/assets/common.js");
  Expect(commonJs.status == 200, "common.js should return 200");
  Expect(commonJs.contentType.find("javascript") != std::string::npos, "common.js should be served as javascript");

  const auto statusBefore = ParseJsonObject(HttpsGet(port, "/api/status").body);
  Expect(statusBefore.contains("running") && statusBefore.at("running").as_bool(), "status should report running=true");
  Expect(statusBefore.at("rooms").as_int64() == 0, "status should start with zero rooms");
  Expect(statusBefore.at("viewers").as_int64() == 0, "status should start with zero viewers");

  WsClient hostWs(port);
  hostWs.Send(R"({"type":"host.register","room":"smoke-room","token":"host-token"})");
  const auto hostRegistered = hostWs.ReadObject();
  Expect(JsonString(hostRegistered, "type") == "host.registered", "host should receive host.registered");

  WsClient viewerWs(port);
  viewerWs.Send(R"({"type":"room.join","room":"smoke-room"})");
  const auto roomJoined = viewerWs.ReadObject();
  Expect(JsonString(roomJoined, "type") == "room.joined", "viewer should receive room.joined");
  const auto viewerPeerId = JsonString(roomJoined, "peerId");
  Expect(!viewerPeerId.empty(), "viewer peer id should be present");

  const auto peerJoined = hostWs.ReadObject();
  Expect(JsonString(peerJoined, "type") == "peer.joined", "host should receive peer.joined");
  Expect(JsonString(peerJoined, "peerId") == viewerPeerId, "peer.joined should reference the viewer id");

  const auto statusJoined = ParseJsonObject(HttpsGet(port, "/api/status").body);
  Expect(statusJoined.at("rooms").as_int64() == 1, "status should report one room after host registration");
  Expect(statusJoined.at("viewers").as_int64() == 1, "status should report one viewer after join");

  hostWs.Send(std::string("{\"type\":\"webrtc.offer\",\"to\":\"") + viewerPeerId +
              "\",\"token\":\"host-token\",\"sdp\":\"fake-offer\"}");
  const auto offer = viewerWs.ReadObject();
  Expect(JsonString(offer, "type") == "webrtc.offer", "viewer should receive forwarded offer");
  Expect(JsonString(offer, "from") == "host", "offer should identify host sender");
  Expect(JsonString(offer, "room") == "smoke-room", "offer should preserve room");
  Expect(JsonString(offer, "sdp") == "fake-offer", "offer should preserve sdp");
  Expect(!offer.contains("token"), "offer should not leak host token");

  viewerWs.Send(std::string("{\"type\":\"webrtc.answer\",\"to\":\"host\",\"sdp\":\"fake-answer\"}"));
  const auto answer = hostWs.ReadObject();
  Expect(JsonString(answer, "type") == "webrtc.answer", "host should receive viewer answer");
  Expect(JsonString(answer, "from") == viewerPeerId, "answer should identify viewer sender");
  Expect(JsonString(answer, "sdp") == "fake-answer", "answer should preserve sdp");

  hostWs.Send(R"({"type":"session.end","room":"smoke-room","token":"host-token","reason":"browser_smoke"})");
  const auto ended = viewerWs.ReadObject();
  Expect(JsonString(ended, "type") == "session.ended", "viewer should receive session.ended");
  Expect(JsonString(ended, "reason") == "browser_smoke", "session.ended should preserve reason");
  const auto ack = hostWs.ReadObject();
  Expect(JsonString(ack, "type") == "session.end.ack", "host should receive session.end.ack");

  viewerWs.Close();
  hostWs.Close();

  bool zeroed = false;
  for (int attempt = 0; attempt < 20; ++attempt) {
    const auto statusAfter = ParseJsonObject(HttpsGet(port, "/api/status").body);
    if (statusAfter.at("rooms").as_int64() == 0 && statusAfter.at("viewers").as_int64() == 0) {
      zeroed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  Expect(zeroed, "status should return to zero rooms/viewers after websocket close");

  host.Stop();
  std::cout << "browser smoke tests passed\n";
  return 0;
}
