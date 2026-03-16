#include "ws_session.h"

#include "core/protocol/json_helpers.h"
#include "core/protocol/messages.h"
#include "ws_hub.h"

#include <boost/system/error_code.hpp>

namespace lan::server {

void WsSession::HandleMessage(const std::string& text) {
  // boost::json::parse reports errors via boost::system::error_code.
  boost::system::error_code jec;
  auto v = boost::json::parse(text, jec);
  if (jec || !v.is_object()) return;

  auto obj = v.as_object(); // copy, so we can safely mutate (e.g. add "from")

  const auto typeOpt = lan::protocol::GetString(obj, "type");
  if (!typeOpt) return;
  const std::string type = *typeOpt;

  // Persist room once provided.
  if (auto r = lan::protocol::GetString(obj, "room")) {
    room_ = *r;
  }

  if (type == "host.register") {
    HandleHostRegister(obj);
    return;
  }

  if (type == "room.join") {
    HandleRoomJoin(obj);
    return;
  }

  if (type == "session.end") {
    HandleSessionEnd(obj);
    return;
  }

  if (type == "webrtc.offer" || type == "webrtc.answer" || type == "webrtc.ice") {
    HandleWebrtcForward(std::move(obj));
    return;
  }
}

bool WsSession::ValidateHostToken(const boost::json::object& obj) const {
  if (!isHost_) return false;
  auto tokIt = obj.find("token");
  if (tokIt == obj.end() || !tokIt->value().is_string()) return false;
  return tokIt->value().as_string() == hostToken_;
}

void WsSession::HandleHostRegister(const boost::json::object& obj) {
  if (!hub_) return;

  const auto room = lan::protocol::GetStringOr(obj, "room");
  const auto token = lan::protocol::GetStringOr(obj, "token");
  if (room.empty() || token.empty()) {
    Send(lan::protocol::Error("BAD_REQUEST", "room/token is required"));
    return;
  }

  isHost_ = true;
  peerId_ = "host";
  room_ = room;
  hostToken_ = token;

  std::string err;
  if (!hub_->RegisterHost(room_, hostToken_, shared_from_this(), err)) {
    Send(lan::protocol::Error(err.empty() ? "HOST_ALREADY_EXISTS" : err, "cannot register host"));
    return;
  }

  Send(lan::protocol::HostRegistered(room_));
}

void WsSession::HandleRoomJoin(const boost::json::object& obj) {
  if (!hub_) return;

  const auto room = lan::protocol::GetStringOr(obj, "room");
  if (room.empty()) {
    Send(lan::protocol::Error("BAD_REQUEST", "room is required"));
    return;
  }

  room_ = room;

  std::string err;
  const auto id = hub_->JoinViewer(room_, shared_from_this(), err);
  if (id.empty()) {
    Send(lan::protocol::Error(err.empty() ? "ROOM_NOT_FOUND" : err, "cannot join room"));
    return;
  }

  peerId_ = id;
  Send(lan::protocol::RoomJoined(room_, peerId_));

  // Notify host
  hub_->NotifyHost(room_, lan::protocol::PeerJoined(room_, peerId_));
}

void WsSession::HandleSessionEnd(const boost::json::object& obj) {
  if (!hub_) return;
  if (!isHost_) return;

  if (!ValidateHostToken(obj)) {
    Send(lan::protocol::Error("UNAUTHORIZED_HOST", "bad token"));
    return;
  }

  std::string reason = lan::protocol::GetStringOr(obj, "reason", "host_stopped");
  if (room_.empty()) {
    Send(lan::protocol::Error("BAD_REQUEST", "room is required"));
    return;
  }

  hub_->EndSession(room_, reason);
  Send(lan::protocol::SessionEndAck(room_));
}

void WsSession::HandleWebrtcForward(boost::json::object obj) {
  if (!hub_) return;

  if (room_.empty()) {
    Send(lan::protocol::Error("BAD_REQUEST", "room is required"));
    return;
  }

  const auto to = lan::protocol::GetStringOr(obj, "to");
  if (to.empty()) return;

  // Host auth for messages requiring token.
  if (isHost_ && !ValidateHostToken(obj)) {
    Send(lan::protocol::Error("UNAUTHORIZED_HOST", "bad token"));
    return;
  }

  // The sender id is maintained by the server.
  obj.erase("token");
  obj["from"] = peerId_;
  obj["room"] = room_;

  hub_->ForwardTo(room_, to, boost::json::serialize(obj));
}

} // namespace lan::server
