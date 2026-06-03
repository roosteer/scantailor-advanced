#!/usr/bin/env bash
set -euo pipefail

# ---------------------------------------------------------------------------
# build-docker-image.sh
#
# Build a Docker image of ScanTailor Advanced CLI.
#
# Usage:
#   ./build-docker-image.sh                              # build with default tag
#   ./build-docker-image.sh --tag myrepo/st:v1           # custom tag
#   ./build-docker-image.sh --no-cache                   # disable build cache
#   ./build-docker-image.sh --push                       # push to registry
#   ./build-docker-image.sh --platform linux/amd64       # single platform
#   ./build-docker-image.sh --platform linux/amd64,linux/arm64  # multi-arch
#
# Default tag: scantailor-advanced:<version> (also tagged "latest").
# Version is read from version.h.in.
#
# Environment:
#   DOCKER_BUILD_OPTS   extra flags passed verbatim to `docker build`
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
readonly SCRIPT_DIR

# ---------------------------------------------------------------------------
# Terminal colors (detect if stdout is a terminal)
# ---------------------------------------------------------------------------
if [[ -t 1 ]]; then
    readonly C_RED='\033[0;31m'
    readonly C_GREEN='\033[0;32m'
    readonly C_YELLOW='\033[1;33m'
    readonly C_BOLD='\033[1m'
    readonly C_RESET='\033[0m'
else
    readonly C_RED='' C_GREEN='' C_YELLOW='' C_BOLD='' C_RESET=''
fi

log_info()  { echo -e "${C_GREEN}==>${C_RESET} ${C_BOLD}$*${C_RESET}"; }
log_warn()  { echo -e "${C_YELLOW}==>${C_RESET} ${C_BOLD}WARNING:${C_RESET} $*"; }
log_error() { echo -e "${C_RED}==>${C_RESET} ${C_BOLD}ERROR:${C_RESET} $*" >&2; }

# ---------------------------------------------------------------------------
# Usage
# ---------------------------------------------------------------------------
show_usage() {
    cat <<EOF
Usage: $0 [options]

Build a Docker image of ScanTailor Advanced CLI.

Options:
  --tag <name>[:tag]     Image tag (default: scantailor-advanced:<version>)
  --no-cache             Disable Docker build cache
  --push                 Push the image to the registry after building
  --platform <platform>  Target platform(s), e.g. linux/amd64 or linux/amd64,linux/arm64
                         Auto-detects buildx and uses it when --platform is given.
  -h, --help             Show this help message and exit

Environment:
  DOCKER_BUILD_OPTS      Extra flags passed verbatim to \`docker build\`
EOF
    exit 0
}

# ---------------------------------------------------------------------------
# Parse CLI arguments
# ---------------------------------------------------------------------------
no_cache=false
push_image=false
platform=""
image_tag=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            show_usage
            ;;
        --tag)
            if [[ $# -lt 2 ]]; then
                log_error "--tag requires an argument"
                exit 1
            fi
            image_tag="$2"
            shift 2
            ;;
        --no-cache)
            no_cache=true
            shift
            ;;
        --push)
            push_image=true
            shift
            ;;
        --platform)
            if [[ $# -lt 2 ]]; then
                log_error "--platform requires an argument"
                exit 1
            fi
            platform="$2"
            shift 2
            ;;
        *)
            log_error "Unknown flag: $1"
            echo "Usage: $0 [--tag <name>] [--no-cache] [--push] [--platform <platform>]"
            exit 1
            ;;
    esac
done

readonly no_cache push_image platform image_tag

# ---------------------------------------------------------------------------
# Extract version from version.h.in
# ---------------------------------------------------------------------------
VERSION=$(sed -n 's/^#define VERSION "\([^"]*\)".*/\1/p' "${SCRIPT_DIR}/version.h.in")
if [[ -z "$VERSION" ]]; then
    log_error "Could not read VERSION from version.h.in"
    exit 1
fi
readonly VERSION

# ---------------------------------------------------------------------------
# Determine final tag(s)
# ---------------------------------------------------------------------------
IMAGE_TAG="${image_tag:-scantailor-advanced:${VERSION}}"
LATEST_TAG="scantailor-advanced:latest"

log_info "Building ScanTailor Advanced ${VERSION}"
log_info "Image tag: ${IMAGE_TAG}"

# ---------------------------------------------------------------------------
# Assemble docker build command
# ---------------------------------------------------------------------------
build_args=(build)

if [[ -n "$platform" ]]; then
    # Multi-platform: use buildx
    build_args=(buildx build)
    build_args+=(--platform "$platform")
    build_args+=(--load)
fi

if [[ "$no_cache" == true ]]; then
    build_args+=(--no-cache)
    log_info "Build cache disabled"
fi

build_args+=(-t "$IMAGE_TAG")
# Only auto-tag "latest" when using the default tag scheme (not a fully-qualified custom tag)
if [[ -z "$image_tag" ]]; then
    build_args+=(-t "$LATEST_TAG")
fi

# Append environment-driven extra flags
if [[ -n "${DOCKER_BUILD_OPTS:-}" ]]; then
    # shellcheck disable=SC2206
    build_args+=($DOCKER_BUILD_OPTS)
fi

build_args+=("${SCRIPT_DIR}")

# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------
log_info "Running: docker ${build_args[*]}"
docker "${build_args[@]}"

log_info "Build complete: ${IMAGE_TAG}"

# ---------------------------------------------------------------------------
# Optionally push
# ---------------------------------------------------------------------------
if [[ "$push_image" == true ]]; then
    if [[ -n "$platform" ]]; then
        log_info "Pushing multi-platform image: ${IMAGE_TAG}"
        docker buildx build --platform "$platform" --push -t "$IMAGE_TAG" "${SCRIPT_DIR}"
    else
        log_info "Pushing image: ${IMAGE_TAG}"
        docker push "$IMAGE_TAG"
    fi
    log_info "Push complete: ${IMAGE_TAG}"
fi
