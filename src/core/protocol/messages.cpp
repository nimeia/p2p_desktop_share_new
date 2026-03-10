#include "messages.h"

#include <boost/json.hpp>

namespace lan::protocol {

static std::string Serialize(const boost::json::object& obj) {
  return boost::json::serialize(obj);
}

std::string Error(std::string_view code, std::string_view message) {
  boost::json::object o;
  o["type"] = "error";
  o["code"] = boost::json::string(code);
  o["message"] = boost::json::string(message);
  return Serialize(o);
}

std::string HostRegistered(std::string_view room) {
  boost::json::object o;
  o["type"] = "host.registered";
  o["room"] = boost::json::string(room);
  return Serialize(o);
}

std::string RoomJoined(std::string_view room, std::string_view peerId) {
  boost::json::object o;
  o["type"] = "room.joined";
  o["room"] = boost::json::string(room);
  o["peerId"] = boost::json::string(peerId);
  return Serialize(o);
}

std::string PeerJoined(std::string_view room, std::string_view peerId) {
  boost::json::object o;
  o["type"] = "peer.joined";
  o["room"] = boost::json::string(room);
  o["peerId"] = boost::json::string(peerId);
  return Serialize(o);
}

std::string PeerLeft(std::string_view room, std::string_view peerId) {
  boost::json::object o;
  o["type"] = "peer.left";
  o["room"] = boost::json::string(room);
  o["peerId"] = boost::json::string(peerId);
  return Serialize(o);
}

std::string SessionEnded(std::string_view room, std::string_view reason) {
  boost::json::object o;
  o["type"] = "session.ended";
  o["room"] = boost::json::string(room);
  o["reason"] = boost::json::string(reason);
  return Serialize(o);
}

std::string SessionEndAck(std::string_view room) {
  boost::json::object o;
  o["type"] = "session.end.ack";
  o["room"] = boost::json::string(room);
  return Serialize(o);
}

std::string ApiStatus(bool running, std::size_t rooms, std::size_t viewers) {
  boost::json::object o;
  o["running"] = running;
  o["rooms"] = rooms;
  o["viewers"] = viewers;
  return Serialize(o);
}

} // namespace lan::protocol
