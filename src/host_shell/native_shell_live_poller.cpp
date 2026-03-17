#include "host_shell/native_shell_live_poller.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace lan::host_shell {
namespace {
namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

std::string ToUtf8(std::wstring_view value) {
  std::string out;
  out.reserve(value.size());
  for (wchar_t ch : value) {
    out.push_back(ch >= 0 && ch < 0x80 ? static_cast<char>(ch) : '?');
  }
  return out;
}

bool ExtractBool(std::string_view json, std::string_view key, bool fallback) {
  const std::string needle = std::string{"\""} + std::string(key) + "\":";
  const auto pos = json.find(needle);
  if (pos == std::string_view::npos) return fallback;
  auto valuePos = pos + needle.size();
  while (valuePos < json.size() && std::isspace(static_cast<unsigned char>(json[valuePos]))) ++valuePos;
  if (json.substr(valuePos, 4) == "true") return true;
  if (json.substr(valuePos, 5) == "false") return false;
  return fallback;
}

std::size_t ExtractUint(std::string_view json, std::string_view key, std::size_t fallback) {
  const std::string needle = std::string{"\""} + std::string(key) + "\":";
  const auto pos = json.find(needle);
  if (pos == std::string_view::npos) return fallback;
  auto valuePos = pos + needle.size();
  while (valuePos < json.size() && std::isspace(static_cast<unsigned char>(json[valuePos]))) ++valuePos;
  std::size_t value = 0;
  bool any = false;
  while (valuePos < json.size() && std::isdigit(static_cast<unsigned char>(json[valuePos]))) {
    any = true;
    value = (value * 10) + static_cast<std::size_t>(json[valuePos] - '0');
    ++valuePos;
  }
  return any ? value : fallback;
}

bool HttpGet(const NativeShellEndpointConfig& config,
             std::string_view target,
             http::response<http::string_body>& response,
             std::string& err) {
  try {
    asio::io_context ioc;
    tcp::resolver resolver(ioc);
    beast::tcp_stream stream(ioc);

    stream.expires_after(std::chrono::milliseconds(config.timeoutMs));
    auto results = resolver.resolve(config.host, std::to_string(config.port));
    stream.connect(results);

    http::request<http::empty_body> req{http::verb::get, std::string(target), 11};
    req.set(http::field::host, config.host);
    req.set(http::field::user_agent, "lan-native-shell");
    http::write(stream, req);

    beast::flat_buffer buffer;
    http::read(stream, buffer, response);

    boost::system::error_code ec;
    beast::get_lowest_layer(stream).socket().shutdown(tcp::socket::shutdown_both, ec);
    if (ec) {
      err = ec.message();
      return false;
    }
    err.clear();
    return true;
  } catch (const std::exception& ex) {
    err = ex.what();
    return false;
  }
}

} // namespace

NativeShellLiveSnapshot PollNativeShellLive(const NativeShellEndpointConfig& config) {
  NativeShellLiveSnapshot snapshot;

  http::response<http::string_body> healthResponse;
  std::string healthErr;
  const bool healthOk = HttpGet(config, "/health", healthResponse, healthErr) &&
                        healthResponse.result() == http::status::ok &&
                        healthResponse.body() == "ok";

  snapshot.runtime.localHealthReady = healthOk;
  snapshot.runtime.serverRunning = healthOk;
  snapshot.runtime.attentionNeeded = !healthOk;
  snapshot.runtime.detailText = healthOk ? L"Local health probe succeeded." : ToWide(healthErr.empty() ? std::string{"Health probe failed."} : healthErr);

  http::response<http::string_body> statusResponse;
  std::string statusErr;
  const bool statusOk = HttpGet(config, "/api/status", statusResponse, statusErr) &&
                        statusResponse.result() == http::status::ok;
  snapshot.statusEndpointReady = statusOk;
  if (statusOk) {
    const auto body = statusResponse.body();
    snapshot.runtime.serverRunning = ExtractBool(body, "running", snapshot.runtime.serverRunning);
    snapshot.rooms = ExtractUint(body, "rooms", 0);
    snapshot.viewers = ExtractUint(body, "viewers", 0);
    snapshot.runtime.viewerCount = snapshot.viewers;
    snapshot.runtime.attentionNeeded = snapshot.runtime.attentionNeeded || !snapshot.runtime.serverRunning;
    snapshot.diagnostic = body;
  } else if (!statusErr.empty()) {
    snapshot.diagnostic = statusErr;
  }

  snapshot.runtime.hostPageState = !snapshot.runtime.serverRunning ? L"stopped"
                                  : (snapshot.viewers > 0 ? L"sharing" : L"ready");
  if (!snapshot.runtime.serverRunning && healthOk) {
    snapshot.runtime.detailText = L"Status endpoint reports that the sharing service is not running.";
  } else if (snapshot.viewers > 0) {
    snapshot.runtime.detailText = std::to_wstring(snapshot.viewers) + L" viewer(s) connected.";
  } else if (snapshot.runtime.serverRunning) {
    snapshot.runtime.detailText = L"Waiting for viewers.";
  }

  return snapshot;
}

} // namespace lan::host_shell
