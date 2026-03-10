# Code Style & Best Practices

This repo is intentionally small, but we still keep a few conventions to make the code easy to maintain.

## C++

- C++ standard: **C++20**
- Formatting: `.clang-format` at repo root
  - Indent: 2 spaces
  - Column limit: 110
- Prefer **RAII** and value-semantics where possible.
- Prefer `std::string`/`std::string_view` and Boost.JSON for JSON payloads.
- Avoid hand-crafted JSON string concatenation (easy to break escaping/quoting).

### Concurrency

- WebSocket sessions (`WsSession`) serialize all read/write handlers through a strand.
- `WsHub` is the shared state; it uses a mutex and prunes expired weak_ptr entries.

## PowerShell

- Scripts run with strict mode.
- Avoid reading undefined variables (initialize early).
- Keep outputs deterministic: write artifacts into `out/...`.

## Docs

- When changing public endpoints or signaling message shapes, update:
  - `docs/SIGNALING_PROTOCOL.md`
  - `docs/DEVELOPMENT.md`
  - `docs/PROGRESS_WIP.md`
