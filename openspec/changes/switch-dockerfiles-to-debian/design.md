## Context

The project's main `Dockerfile` produces a CLI-only image via multi-stage build on `fedora:latest`. The `.devcontainer/Dockerfile` is already Debian-based (`debian:bookworm-slim`). Both serve different purposes — CI-less containerized CLI invocation vs interactive development — but use different package ecosystems for the same underlying libraries.

Current `Dockerfile` stages:
1. **builder**: Fedora with -devel packages, cmake, g++, Qt5, Boost, image libraries
2. **runtime**: Fedora with runtime shared libraries only, offscreen Qt platform

The `.devcontainer/Dockerfile` installs the GUI + test toolchain (gdb, libgl1-mesa-dev, libboost-test-dev, git).

## Goals / Non-Goals

**Goals:**
- Convert `Dockerfile` from Fedora → Debian bookworm-slim with correct package mapping
- Align `.devcontainer/Dockerfile` idioms to match (comments, `--no-install-recommends` consistency)
- Both images must produce the same binary output as before
- Builder stage must produce a working `scantailor-advanced-cli` binary
- Runtime stage must be minimal: only shared libraries needed at runtime

**Non-Goals:**
- Changing the build system, CMake configuration, or compiler flags
- Adding GUI support to the main Dockerfile (stays CLI-only)
- Reducing the dependency set beyond what Fedora already ships (explore in a later optimization pass)
- Changing CI workflows (already on Ubuntu)

## Decisions

### 1. Base image: `debian:bookworm-slim`

| Alternative | Verdict |
|---|---|
| `debian:bookworm-slim` | **Adopted.** Matches the existing devcontainer. Proven toolchain. Qt 5.15, gcc 12, CMake 3.25. |
| `debian:trixie-slim` | Rejected — newer but diverges from devcontainer; no benefit for this change. |
| `debian:stable-slim` | Rejected — floating tag risks non-reproducible builds. Pin to a named release. |
| `ubuntu:24.04` (jammy) | Rejected — same libc version roughly, but devcontainer uses Debian; consistency wins. |
| Keep Fedora | Rejected — the whole point of this change is consistency. |

### 2. Build toolchain: `build-essential` meta-package

Fedora's individual `gcc-c++` + `make` are replaced by Debian's `build-essential`, which pulls in `g++`, `make`, `libc-dev`, and `dpkg-dev`. This is the standard Debian pattern and matches the devcontainer.

### 3. Boost header-only: `libboost-dev`

The project uses Boost headers (multi_index, etc.) but only needs the unit_test_framework compiled library when `BUILD_TESTS=ON`. The Dockerfile builds with `-DBUILD_TESTS=OFF`, so `libboost-dev` (headers only) is sufficient in the builder and `libboost-test-dev` is unnecessary.

### 4. Runtime Qt packages: split by library

Fedora bundles Qt runtime into `qt5-qtbase` + `qt5-qtbase-gui` + `qt5-qtsvg`. Debian has per-library packages: `libqt5core5a`, `libqt5gui5`, `libqt5widgets5`, `libqt5svg5`, `libqt5xml5`, `libqt5network5`. These are direct functional equivalents.

### 5. Image libraries

| Fedora | Debian | Notes |
|---|---|---|
| `libjpeg-turbo` | `libjpeg62-turbo` | Debian `libjpeg-dev` installs libjpeg-turbo headers |
| `libpng` | `libpng16-16` | |
| `libtiff` | `libtiff6` | |
| `zlib` | `zlib1g` | |

### 6. `Dockerfile` is already multi-stage; keep that structure

The build stage installs -dev packages. The runtime stage installs only the runtime shared libraries, plus `libc6`, `libstdc++6` implicitly via base image. The builder's dev packages never leak into the final image.

### 7. `.devcontainer/Dockerfile` alignment

No functional changes needed. Only cosmetic alignment:
- Use `--no-install-recommends` consistently (already does)
- Add `&& rm -rf /var/lib/apt/lists/*` to the RUN layer (already does)
- Update comment to clarify it's aligned with the main Dockerfile's Debian base

## Package Mapping Reference

### Build stage: Fedora → Debian

```
cmake                     → cmake
gcc-c++ make              → build-essential (meta: g++, make, libc-dev, dpkg-dev)
qt5-qtbase-devel          → qtbase5-dev, libqt5opengl5-dev
qt5-qtsvg-devel           → libqt5svg5-dev
qt5-qttools-devel         → qttools5-dev
boost-devel               → libboost-dev
boost-test                → (not needed — BUILD_TESTS=OFF)
libjpeg-turbo-devel       → libjpeg-dev
libpng-devel              → libpng-dev
libtiff-devel             → libtiff-dev
zlib-devel                → zlib1g-dev
```

### Runtime stage: Fedora → Debian

```
qt5-qtbase                → libqt5core5a, libqt5gui5, libqt5widgets5, libqt5xml5, libqt5network5, libqt5opengl5
qt5-qtbase-gui            → (included in libqt5gui5)
qt5-qtsvg                 → libqt5svg5
libjpeg-turbo             → libjpeg62-turbo
libpng                    → libpng16-16
libtiff                   → libtiff6
zlib                      → zlib1g
```

### Runtime packages from base image (no explicit install needed)
- `libc6`, `libstdc++6`, `libgcc-s1` — provided by `debian:bookworm-slim`

## Risks / Trade-offs

| Risk | Mitigation |
|---|---|
| **glibc mismatch**: Binary built on bookworm (glibc 2.36) might not run on older hosts | This is the same constraint as building on Ubuntu 24.04 CI; bookworm's glibc is older (2.36 vs 2.39 in Ubuntu 24.04), so the binary is more portable, not less |
| **Missing library**: A runtime library present on Fedora but absent or differently named on Debian | Verified mapping against CI's apt install list; build-deb.sh Depends fallback also confirms the names |
| **Qt platform plugin**: `QT_QPA_PLATFORM=offscreen` may behave differently | libqt5gui5 on Debian includes the offscreen plugin; verified via `ldd` and existing CI usage |
| **apt layer size**: `apt-get update` + `install` without cleanup leaves cache | Pattern: always `&& rm -rf /var/lib/apt/lists/*` in the same RUN layer |
| **apt non-interactive**: Interactive prompts block build | Set `DEBIAN_FRONTEND=noninteractive` before apt calls |
