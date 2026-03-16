# Desktop Host (WIP) - Build Guide

This is a **work-in-progress** wizard project.

## 1) Build the C++ server
From repo root:

- Configure and build:
  - `cmake -S . -B out/build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`
  - `cmake --build out/build --config Release`

Targets:
- `lan_screenshare_server` (from `apps/server_cli`)
- `lan_demo` (if existing)

## 2) Generate certificate
- `scripts/gen_self_signed_cert.ps1 -OutDir .\cert -SanIp <host-ip>`

Notes:
- The server-side certificate path now uses in-process OpenSSL generation; desktop trust/bootstrap UX still needs follow-up polish.
- Recommended: set `VCPKG_ROOT` (or use `scripts\windows\build.ps1`, which passes it automatically).

## 3) Run server (manual, WIP)
- `out\server\Release\lan_screenshare_server.exe --bind 0.0.0.0 --port 9443 --www out\server\Release\www --certdir out\server\Release\cert --san-ip <host-ip>`



## 4) Build/run desktop host
Open:
- `src/desktop_host/LanScreenShareHostApp.sln`

Build and run project `LanScreenShareHostApp`.

Or use scripts:
- Build: `scripts\windows\build.ps1 -Target desktop_host -Config Debug`
- Run: `scripts\windows\run_desktop_host.ps1 -Config Debug`

Runtime output directories:
- server: `out\server\<Config>\`
- desktop host: `out\desktop_host\<Arch>\<Config>\`


`--san-ip auto` is now supported on the CLI. On Windows it resolves via the Windows network provider; on Linux/macOS it resolves via the POSIX provider and falls back to loopback when discovery is unavailable.
