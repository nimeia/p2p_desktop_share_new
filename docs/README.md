# Documentation Index

This folder now mixes two kinds of documents:

- active operator/developer guides that should match the current codebase
- historical plans/specs that preserve refactor context but may intentionally describe removed or superseded flows

## Start here

- [../README.md](../README.md): repository overview and quick-start commands
- [BUILD_WINDOWS.md](BUILD_WINDOWS.md): current Windows build and validation workflow
- [WINDOWS_PACKAGING.md](WINDOWS_PACKAGING.md): zip packaging and install/uninstall flow
- [WINDOWS_STORE_PACKAGING.md](WINDOWS_STORE_PACKAGING.md): MSIX / Store packaging flow
- [WINDOWS_BOOTSTRAP_GUIDE.md](WINDOWS_BOOTSTRAP_GUIDE.md): current Windows operator checks
- [RELEASE_VALIDATION.md](RELEASE_VALIDATION.md): browser smoke plus desktop payload validation

## Current system docs

- [ARCHITECTURE.md](ARCHITECTURE.md): current runtime shape and responsibility split
- [DEVELOPMENT.md](DEVELOPMENT.md): implementation baseline and current priorities
- [FUNCTIONAL_SPEC.md](FUNCTIONAL_SPEC.md): product-facing behavior and expectations
- [UNFINISHED_FEATURES.md](UNFINISHED_FEATURES.md): current open work list
- [SIGNALING_PROTOCOL.md](SIGNALING_PROTOCOL.md): WebSocket message contract
- [CODE_STYLE.md](CODE_STYLE.md): coding conventions

## Desktop host docs

- [desktop_host/BUILD_GUIDE.md](desktop_host/BUILD_GUIDE.md): current desktop host build/run guide
- [desktop_host/BUILD_STATUS.md](desktop_host/BUILD_STATUS.md): current desktop-host baseline and known gaps

## Historical context

The files below are still useful, but treat them as historical snapshots unless a newer active guide links to them directly:

- `CROSS_PLATFORM_REFACTOR_PLAN.md`
- `DEVELOPMENT_PLAN.md`
- `MAINWINDOW_REMAINING_RESPONSIBILITIES.md`
- `PROGRESS_WIP.md`
- most files under `docs/desktop_host/` with names ending in `_PLAN.md`, `_SPEC.md`, or `_TASKS.md`

These files may mention completed refactors, old transport assumptions, or removed flows such as the former TLS/certificate bootstrap path.
