## ADDED Requirements

### Requirement: Dockerfile must be Debian-based
The main `Dockerfile` and `.devcontainer/Dockerfile` MUST use a Debian base image (`debian:bookworm-slim`). All build toolchain and runtime packages MUST be Debian equivalents.

#### Scenario: Build succeeds on Debian
- **WHEN** the `Dockerfile` is built with `docker build .`
- **THEN** it exits successfully and produces a `scantailor-advanced-cli` binary

#### Scenario: Binary runs and prints help
- **WHEN** the resulting image is run with `docker run <image>`
- **THEN** it prints usage help and exits with code 0

#### Scenario: Devcontainer opens on Debian
- **WHEN** the devcontainer is built
- **THEN** it installs all GUI build dependencies and presents a working build environment

## MODIFIED Requirements

None — this is an infrastructure-only change. No behavioral requirements are modified.

## REMOVED Requirements

None.

## RENAMED Requirements

None.
