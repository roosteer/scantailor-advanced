## 1. Convert main Dockerfile to Debian

- [x] 1.1 Replace `FROM fedora:latest AS builder` → `FROM debian:bookworm-slim AS builder`
- [x] 1.2 Replace `FROM fedora:latest` (runtime) → `FROM debian:bookworm-slim`
- [x] 1.3 Translate build stage package installation: `dnf install` → `apt-get` with `--no-install-recommends`
- [x] 1.4 Translate runtime stage package installation: `dnf install` → `apt-get`
- [x] 1.5 Set `DEBIAN_FRONTEND=noninteractive` before apt calls
- [x] 1.6 Add `rm -rf /var/lib/apt/lists/*` cleanup to both RUN layers
- [x] 1.7 Verify cmake flag `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE` is preserved

## 2. Align devcontainer Dockerfile

- [x] 2.1 Update comment header to reference Debian base alignment
- [x] 2.2 Add `--no-install-recommends` if missing
- [x] 2.3 Verify `DEBIAN_FRONTEND=noninteractive` / cleanup pattern is correct

## 3. Verify the build

- [x] 3.1 Run `docker build .` with the new Dockerfile and confirm exit code 0
- [x] 3.2 Run `docker build -f .devcontainer/Dockerfile .` and confirm exit code 0
- [x] 3.3 Run the resulting image: `docker run <image>` prints help and exits 0
- [x] 3.4 Confirm `strip` was applied to the binary (check size)
