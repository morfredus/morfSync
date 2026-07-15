# HomeServerHub

*Read in another language: **English** (this document) · [Français](README.fr.md).*

![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20Raspberry%20Pi-lightgrey)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Build](https://img.shields.io/badge/CMake-3.21+-064F8C?logo=cmake)
![License](https://img.shields.io/badge/License-GPL--3.0--only-blue)

**HomeServerHub** is the offline-first synchronization backbone of the *morf*
ecosystem (ComponentHub, MeteoHub, RaspberryDashboard, SiteWatch…). It is **not**
a database. Each client keeps its own local copy of the data and works on it
without any network. The hub only exists to keep every copy consistent on the
local network.

> One architecture for every project: the same sync **contract** (envelope +
> REST endpoints) applies to a desktop app or a memory-constrained ESP32.

## How it works (in one paragraph)

Every syncable entity carries an envelope — `{ id (UUID), rev, updatedAt,
deleted, origin }` — around an opaque business payload. Clients **PUSH** their
local changes, then **PULL** everything since their cursor (`GET
/changes?since=N`). The hub assigns a **monotonic sequence number** that orders
everything and resolves conflicts as last-write-wins — **it never compares wall
clocks between machines** (the ESP32 drifts, the Raspberry Pi has no RTC). The
full specification lives in [docs/sync-contract.md](docs/sync-contract.md).

## API

| Method | Endpoint | Purpose |
|--------|----------|---------|
| `GET`  | `/api/health` | Server status (open, no auth) |
| `GET`  | `/api/status` | Known domains: entities, `lastSeq` cursor and `journalId` (epoch) |
| `GET`  | `/api/{domain}/changes?since=N&limit=M` | PULL changes since cursor |
| `POST` | `/api/{domain}/changes` | PUSH local changes, get assigned `seq` |

`{domain}` is one journal per project (`componenthub`, `meteohub`…), created on
demand. Optional shared-token auth via `Authorization: Bearer <token>`.

## Design choices

- **Zero web framework.** A minimal, self-contained HTTP/1.1 server (winsock2 /
  POSIX sockets). Only external dependency: `nlohmann_json` — the same one
  ComponentHub already uses. Compiles independently on win-x64, Linux and ARM64.
- **Business-agnostic.** The hub transports envelopes; it never interprets the
  `data` payload. Adding a new project needs no server change.
- **Storage abstraction ready.** Journals are JSON files today; a future SQLite
  backend replaces them without touching clients.

## Building

Requires CMake ≥ 3.21, a C++17 compiler and `nlohmann_json`.

### Windows (MSYS2 / MinGW)
```bash
pacman -S mingw-w64-x86_64-nlohmann-json mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
cmake --preset mingw
cmake --build --preset mingw
# -> build-mingw/HomeServerHub.exe
```

### Linux (x86_64) / Raspberry Pi (ARM64, native)
```bash
sudo apt install nlohmann-json3-dev cmake ninja-build
cmake --preset linux           # or: --preset linux-arm64 on the Pi itself
cmake --build --preset linux
# -> build/HomeServerHub
```

### Linux ARM64 (cross-compiled from x86_64)
```bash
export HSH_SYSROOT=/path/to/arm64/sysroot   # must provide nlohmann_json headers
cmake --preset linux-arm64-cross
cmake --build --preset linux-arm64-cross
```

### Run the smoke tests (no network)
```bash
cmake --preset linux -DHSH_BUILD_SMOKE=ON
cmake --build --preset linux
ctest --preset linux
```

## Running

```bash
cp config.example.json config.json    # then set a token if desired
./HomeServerHub                       # or: ./HomeServerHub /path/to/config.json
```
Quick check: `curl http://localhost:8080/api/health`

**Install as a service / auto-start** (Linux and Windows), listen configuration
(host/port), firewall: see [docs/INSTALLATION.md](docs/INSTALLATION.md). Absolute
beginner walkthrough: [docs/GUIDE_DEBUTANT.md](docs/GUIDE_DEBUTANT.md) (French).

- Linux: `sudo ./scripts/linux/install-service.sh`
- Windows (admin PowerShell): `.\scripts\windows\install-service.ps1`

## Repository layout

```
src/net/      minimal cross-platform HTTP server
src/sync/     change envelope + ordered journal (ChangeStore)
src/app/      configuration
src/main.cpp  wires the contract endpoints to the journal
docs/         the synchronization contract (source of truth)
cmake/        ARM64 cross toolchain
scripts/      packaging / service files
test/         headless smoke test
```

## License

GPL-3.0-only — see [LICENSE](LICENSE).
