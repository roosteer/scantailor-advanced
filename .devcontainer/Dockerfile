# Devcontainer for ScanTailor Advanced
# Based on debian:bookworm-slim, aligned with the main Dockerfile (both Debian).

FROM debian:bookworm-slim

# Prevent apt from prompting for configuration
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build toolchain
    build-essential \
    cmake \
    gdb \
    # Qt5
    qtbase5-dev \
    libqt5svg5-dev \
    qttools5-dev \
    # Boost (headers + unit test framework)
    libboost-dev \
    libboost-test-dev \
    # Image format libraries
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
    zlib1g-dev \
    # OpenGL support for Qt
    libgl1-mesa-dev \
    # General utilities
    git \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ENV DEBIAN_FRONTEND=
