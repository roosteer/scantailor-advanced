#!/usr/bin/env bash
# Build a Docker image of ScanTailor Advanced CLI.
# Usage: ./build-docker-image.sh [image_tag]
# The default tag is "scantailor-advanced:<version>" (also tagged "latest").

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Extract version from version.h.in
VERSION=$(sed -n 's/^#define VERSION "\([^"]*\)".*/\1/p' version.h.in)
if [[ -z "$VERSION" ]]; then
  echo "Error: could not read VERSION from version.h.in" >&2
  exit 1
fi

IMAGE_TAG="${1:-scantailor-advanced:${VERSION}}"

echo "Building ScanTailor Advanced ${VERSION} Docker image: ${IMAGE_TAG}"

docker build -t "$IMAGE_TAG" -t "scantailor-advanced:latest" .

echo "Done: ${IMAGE_TAG}"
