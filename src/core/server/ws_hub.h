#pragma once
#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>
#include <cstddef>

namespace lan::server {

class WsSession;


struct HubStats {
  size_t rooms = 0;
  size_t viewers = 0;
};

struct RoomState {
  std::weak_ptr<WsSession> host;
  std::string hostToken;
  std::unordered_map<std::string, std::weak_ptr<WsSession>> viewers; // peerId -> session
};

class WsHub : public std::enable_shared_from_this<WsHub> {
public:
  explicit WsHub(std::size_t maxViewersPerRoom = 10) : maxViewersPerRoom_(maxViewersPerRoom) {}

  // Host registration
  bool RegisterHost(const std::string& room, const std::string& token, std::shared_ptr<WsSession> session, std::string& err);

  // Viewer join returns assigned peerId
  std::string JoinViewer(const std::string& room, std::shared_ptr<WsSession> session, std::string& err);

  // Cleanup
  void Leave(const std::string& room, const std::string& peerId);

  // Forward JSON text to target
  void ForwardTo(const std::string& room, const std::string& toPeerId, const std::string& text);

  // Notify host about join/leave
  void NotifyHost(const std::string& room, const std::string& text);

  // Broadcast to all viewers in a room
  void BroadcastViewers(const std::string& room, const std::string& text);

  // End a session and notify viewers (used for host stop/disconnect)
  void EndSession(const std::string& room, const std::string& reason);

  HubStats GetStats() const;

private:
  // Mutable: allows locking in logically-const operations (e.g., GetStats()).
  mutable std::mutex mu_;
  std::unordered_map<std::string, RoomState> rooms_;

  std::size_t maxViewersPerRoom_ = 10;

  static std::string GenViewerId();
};

} // namespace lan::server
