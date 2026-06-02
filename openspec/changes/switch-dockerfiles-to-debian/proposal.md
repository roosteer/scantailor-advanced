## Why

The project currently has two Dockerfiles with divergent base distributions: the main `Dockerfile` uses Fedora (`dnf`), while `.devcontainer/Dockerfile` already uses Debian (apt). This inconsistency forces maintainers to juggle two package ecosystems. Standardizing on Debian aligns the CI (Ubuntu), packaging (`.deb`), devcontainer, and Docker image under one package manager, reducing maintenance surface and making the Docker image more accessible to the Debian/Ubuntu user base.

## What Changes

- **`Dockerfile`**: Migrate from `fedora:latest` (dnf) to `debian:bookworm-slim` (apt) for both build and runtime stages. Translate all Fedora package names to their Debian equivalents.
- **`.devcontainer/Dockerfile`**: Already Debian-based; minor alignment (consistent `--no-install-recommends` idiom, comment cleanup). No package changes needed.
- **CI is unaffected** (already runs on `ubuntu-latest` with apt).
- **No breaking changes**: The produced CLI binary and runtime behavior are identical. Only the container build infrastructure changes.

## Capabilities

No new or modified capabilities — this is a build infrastructure change, not a feature change. The software's behavior, API, and requirements are untouched.

## Impact

- **`Dockerfile`**: Base image, build toolchain, and runtime packages all replaced with Debian equivalents per the mapping established in design.md.
- **`.devcontainer/Dockerfile`**: Cosmetic alignment only.
- **Dependencies**: Package names change (Fedora → Debian), but the underlying libraries (libjpeg-turbo, libpng, libtiff, zlib, Boost, Qt 5.15) are the same versions available on Debian bookworm.
- **CI / CD**: No change. The `.deb` is already built on `ubuntu-latest`.
- **End users**: No impact. The Docker image produces the same binary.
