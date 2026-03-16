# Windows Bootstrap Guide

This guide covers the two user-visible bootstrap requirements for the current Windows-first product baseline:

1. WebView2 Runtime availability
2. Local HTTPS certificate trust for the desktop-host preview/browser flow

## WebView2 Runtime

The desktop host now treats the Evergreen WebView2 Runtime as the supported runtime model.

Use the built-in helper to inspect the standard runtime registry locations and save a report:

```powershell
.\scripts\windows\Check-WebView2Runtime.ps1
```

From the packaged app, the same helper is shipped under `scripts/windows/Check-WebView2Runtime.ps1`.

Expected outcomes:

- **Runtime detected**: the embedded HTML admin / host preview can initialize when the rest of the environment is healthy.
- **Runtime missing**: install or repair Evergreen WebView2 Runtime, then relaunch the desktop host.

## Local certificate trust

The desktop host still uses a locally generated certificate for the HTTPS/WSS loopback/LAN flow.

The trust helper imports the generated certificate into `Cert:\CurrentUser\Root`:

```powershell
.\scripts\windows\Trust-LocalCertificate.ps1 -CertPath .\out\desktop_host\x64\Release\cert\server.crt
```

From the packaged app, the same helper is shipped under `scripts/windows/Trust-LocalCertificate.ps1`.

## Certificate bypass policy

The embedded WebView2 preview and the Windows-side local probes no longer bypass certificate validation for arbitrary HTTPS targets.

Current policy:

- allow local self-signed certificate bypass only for **loopback** and **private-LAN** URLs
- do **not** bypass certificate validation for public hostnames or public IP targets

This keeps the first-run local preview workable without turning the app into a general-purpose TLS bypass client.
