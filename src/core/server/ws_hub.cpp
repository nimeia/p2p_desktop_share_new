#include "ws_hub.h"

#include "core/protocol/messages.h"
#include "ws_session.h"

#include <random>
#include <vector>

namespace lan::server {

namespace {

std::string RandHex(std::size_t n) {
  static constexpr char k[] = "0123456789abcdef";
  static thread_local std::mt19937 gen{std::random_device{}()};
  std::uniform_int_distribution<int> dis(0, 15);

  std::string s;
  s.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    s.push_back(k[dis(gen)]);
  }
  return s;
}

std::size_t PruneExpiredViewers(RoomState& rs) {
  std::size_t active = 0;
  for (auto it = rs.viewers.begin(); it != rs.viewers.end();) {
    if (it->second.expired()) {
      it = rs.viewers.erase(it);
      continue;
    }
    ++active;
    ++it;
  }
  return active;
}

} // namespace

std::string WsHub::GenViewerId() {
  return "v-" + RandHex(6);
}

bool WsHub::RegisterHost(const std::string& room,
                         const std::string& token,
                         std::shared_ptr<WsSession> session,
                         std::string& err) {
  if (room.empty()) {
    err = "BAD_REQUEST";
    return false;
  }

  std::lock_guard<std::mutex> lk(mu_);
  auto& rs = rooms_[room];
  if (!rs.host.expired()) {
    err = "HOST_ALREADY_EXISTS";
    return false;
  }

  rs.host = std::move(session);
  rs.hostToken = token;
  PruneExpiredViewers(rs);
  return true;
}

std::string WsHub::JoinViewer(const std::string& room,
                             std::shared_ptr<WsSession> session,
                             std::string& err) {
  if (room.empty()) {
    err = "BAD_REQUEST";
    return {};
  }

  std::lock_guard<std::mutex> lk(mu_);
  auto it = rooms_.find(room);
  if (it == rooms_.end() || it->second.host.expired()) {
    err = "ROOM_NOT_FOUND";
    return {};
  }

  auto& rs = it->second;
  const std::size_t active = PruneExpiredViewers(rs);
  if (active >= maxViewersPerRoom_) {
    err = "ROOM_FULL";
    return {};
  }

  const auto peerId = GenViewerId();
  rs.viewers[peerId] = std::move(session);
  return peerId;
}

void WsHub::Leave(const std::string& room, const std::string& peerId) {
  std::shared_ptr<WsSession> host;
  std::vector<std::shared_ptr<WsSession>> viewers;

  {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(room);
    if (it == rooms_.end()) return;

    auto& rs = it->second;
    host = rs.host.lock();

    if (peerId == "host") {
      // Collect viewers for end notification
      for (auto& kv : rs.viewers) {
        if (auto s = kv.second.lock()) viewers.push_back(std::move(s));
      }
      rs.host.reset();
      rs.viewers.clear();
    } else {
      rs.viewers.erase(peerId);
    }

    // Erase room if no host and no viewers
    if (rs.host.expired() && rs.viewers.empty()) {
      rooms_.erase(it);
    }
  }

  if (peerId == "host") {
    // Host disconnected: tell viewers session ended
    const std::string msg = lan::protocol::SessionEnded(room, "host_disconnected");
    for (auto& v : viewers) v->Send(msg);
  } else {
    // Viewer left: notify host
    if (host) host->Send(lan::protocol::PeerLeft(room, peerId));
  }
}

void WsHub::ForwardTo(const std::string& room, const std::string& toPeerId, const std::string& text) {
  std::shared_ptr<WsSession> dst;

  {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(room);
    if (it == rooms_.end()) return;

    if (toPeerId == "host") {
      dst = it->second.host.lock();
    } else {
      auto vit = it->second.viewers.find(toPeerId);
      if (vit != it->second.viewers.end()) dst = vit->second.lock();
    }
  }

  if (dst) dst->Send(text);
}

void WsHub::NotifyHost(const std::string& room, const std::string& text) {
  ForwardTo(room, "host", text);
}

void WsHub::BroadcastViewers(const std::string& room, const std::string& text) {
  std::vector<std::shared_ptr<WsSession>> viewers;
  {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = rooms_.find(room);
    if (it == rooms_.end()) return;
    for (auto& kv : it->second.viewers) {
      if (auto s = kv.second.lock()) viewers.push_back(std::move(s));
    }
  }
  for (auto& v : viewers) v->Send(text);
}

void WsHub::EndSession(const std::string& room, const std::string& reason) {
  BroadcastViewers(room, lan::protocol::SessionEnded(room, reason));
}

HubStats WsHub::GetStats() const {
  HubStats st{};
  std::lock_guard<std::mutex> lk(mu_);
  for (auto const& kv : rooms_) {
    if (kv.second.host.expired()) continue;
    st.rooms += 1;
    for (auto const& vk : kv.second.viewers) {
      if (!vk.second.expired()) st.viewers += 1;
    }
  }
  return st;
}

} // namespace lan::server
