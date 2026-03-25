# Signaling Protocol

## Transport

- WebSocket over `ws://<host>:<port>/ws`
- UTF-8 JSON objects

The host and viewer pages themselves are served over plain HTTP on the same local service.

## Model

- one room has exactly one host
- a room can have multiple viewers
- host peer id is fixed as `"host"`
- viewer peer ids are assigned by the server as `v-<n>` or another `v-...` identifier

## Implemented message types

### Host registration

Host -> server:

```json
{ "type": "host.register", "room": "ABCD", "token": "HOST_TOKEN" }
```

Server -> host:

```json
{ "type": "host.registered", "room": "ABCD" }
```

### Viewer join

Viewer -> server:

```json
{ "type": "room.join", "room": "ABCD" }
```

Server -> viewer:

```json
{ "type": "room.joined", "room": "ABCD", "peerId": "v-1" }
```

Server -> host:

```json
{ "type": "peer.joined", "room": "ABCD", "peerId": "v-1" }
```

### Viewer leave

Server -> host:

```json
{ "type": "peer.left", "room": "ABCD", "peerId": "v-1" }
```

### Session end

Host -> server:

```json
{ "type": "session.end", "room": "ABCD", "token": "HOST_TOKEN", "reason": "host_stopped" }
```

Server -> host:

```json
{ "type": "session.end.ack", "room": "ABCD" }
```

Server -> viewers:

```json
{ "type": "session.ended", "room": "ABCD", "reason": "host_stopped" }
```

### WebRTC forward

Host offer:

```json
{ "type": "webrtc.offer", "room": "ABCD", "token": "HOST_TOKEN", "to": "v-1", "sdp": "..." }
```

Viewer answer:

```json
{ "type": "webrtc.answer", "room": "ABCD", "to": "host", "sdp": "..." }
```

ICE from either side:

```json
{
  "type": "webrtc.ice",
  "room": "ABCD",
  "to": "host",
  "candidate": {
    "candidate": "...",
    "sdpMid": "0",
    "sdpMLineIndex": 0
  }
}
```

The server injects:

- `from`
- canonical `room`

## Error messages

Server -> client:

```json
{ "type": "error", "code": "BAD_REQUEST", "message": "room is required" }
```

Codes currently observed in implementation:

- `BAD_REQUEST`
- `HOST_ALREADY_EXISTS`
- `ROOM_NOT_FOUND`
- `UNAUTHORIZED_HOST`

## Notes

- host-authenticated messages require the `token` field
- viewer messages currently do not require a token
- the protocol is intentionally small and only covers local session/signaling needs
