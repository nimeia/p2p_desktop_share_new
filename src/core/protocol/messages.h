#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace lan::protocol {

// Message builders for WS signaling + simple HTTP JSON endpoints.
// We build JSON via Boost.JSON to avoid escaping bugs and keep maintenance easy.

std::string Error(std::string_view code, std::string_view message);
std::string HostRegistered(std::string_view room);
std::string RoomJoined(std::string_view room, std::string_view peerId);
std::string PeerJoined(std::string_view room, std::string_view peerId);
std::string PeerLeft(std::string_view room, std::string_view peerId);
std::string SessionEnded(std::string_view room, std::string_view reason);
std::string SessionEndAck(std::string_view room);

std::string ApiStatus(bool running, std::size_t rooms, std::size_t viewers);

} // namespace lan::protocol
