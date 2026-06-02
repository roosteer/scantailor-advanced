# Stage 1 — build
FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake build-essential \
    qtbase5-dev libqt5svg5-dev qttools5-dev libqt5opengl5-dev \
    libboost-dev \
    libjpeg-dev libpng-dev libtiff-dev zlib1g-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN mkdir build && cd build \
 && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE \
 && make scantailor-advanced-cli -j$(nproc) VERBOSE=1 \
 && strip scantailor-advanced-cli

# Stage 2 — runtime
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt5core5a libqt5gui5 libqt5widgets5 libqt5xml5 libqt5network5 libqt5opengl5 \
    libqt5svg5 \
    libjpeg62-turbo libpng16-16 libtiff6 zlib1g \
 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/scantailor-advanced-cli /usr/local/bin/
ENV QT_QPA_PLATFORM=offscreen

ENTRYPOINT ["scantailor-advanced-cli"]
CMD ["--help"]
