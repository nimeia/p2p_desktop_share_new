# Desktop Host (WIP) - Build Guide

This is a **work-in-progress** wizard project.

## 1) Build the C++ server
From repo root:

- Configure and build:
  - `cmake -S . -B out/build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`
  - `cmake --build out/build --config Release`

Targets:
- `lan_screenshare_server` (new, WIP)
- `lan_demo` (if existing)

## 2) Generate certificate
- `scripts/gen_self_signed_cert.ps1 -OutDir .\cert -SanIp <host-ip>`

Notes:
- The script will try to find `openssl.exe` automatically (PATH -> vcpkg tool -> common locations).
- Recommended: set `VCPKG_ROOT` (or use `scripts\windows\build.ps1`, which passes it automatically).

## 3) Run server (manual, WIP)
- `out\build\Release\lan_screenshare_server.exe --bind 0.0.0.0 --port 9443 --www www --certdir cert --san-ip <host-ip>`



## 4) Build/run desktop host
Open:
- `desktop_host/LanScreenShareHostApp/LanScreenShareHostApp.sln`

Build and run project `LanScreenShareHostApp`.

Or use scripts:
- Build: `scripts\windows\build.ps1 -Target desktop_host -Config Debug`
- Run: `scripts\windows\run_desktop_host.ps1 -Config Debug`
