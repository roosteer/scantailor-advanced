# Stage 1 — build
FROM fedora:latest AS builder

RUN dnf install -y --setopt=install_weak_deps=False \
    cmake gcc-c++ make \
    qt5-qtbase-devel qt5-qtsvg-devel qt5-qttools-devel \
    boost-devel boost-test \
    libjpeg-turbo-devel libpng-devel libtiff-devel zlib-devel \
 && dnf clean all

WORKDIR /src
COPY . .

RUN mkdir build && cd build \
 && cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Qt6=TRUE \
 && make scantailor-advanced-cli -j$(nproc) VERBOSE=1 \
 && strip scantailor-advanced-cli

# Stage 2 — runtime
FROM fedora:latest

RUN dnf install -y --setopt=install_weak_deps=False \
    qt5-qtbase qt5-qtbase-gui qt5-qtsvg \
    libjpeg-turbo libpng libtiff zlib \
 && dnf clean all

COPY --from=builder /src/build/scantailor-advanced-cli /usr/local/bin/
ENV QT_QPA_PLATFORM=offscreen

ENTRYPOINT ["scantailor-advanced-cli"]
CMD ["--help"]
