#!/bin/bash
set -euo pipefail

SCRIPT="$0"
DEVICES=("qemu" "qotom")
MODES=("debug" "release")

# Defaults
MODE="release"
DEVICE="qemu"
RUN_TESTS="${RUN_TESTS:-1}"
BUILD_ALL=0

# Parse arguments (support both --flag and positional styles)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)
            MODE="${2:-}"
            shift 2
            ;;
        --device)
            DEVICE="${2:-}"
            shift 2
            ;;
        --no-tests)
            RUN_TESTS="0"
            shift
            ;;
        --all)
            BUILD_ALL=1
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# Build all variants if --all is specified
if [[ "$BUILD_ALL" == "1" ]]; then
    echo "Building all variants..."
    for device in "${DEVICES[@]}"; do
        for mode in "${MODES[@]}"; do
            echo "========================================"
            if [[ "$RUN_TESTS" == "0" ]]; then
                "$SCRIPT" --mode "$mode" --device "$device" --no-tests
            else
                "$SCRIPT" --mode "$mode" --device "$device"
            fi
        done
    done
    exit 0
fi

# Validate inputs
case "$MODE" in
    debug) CMAKE_BUILD_TYPE="Debug" ;;
    release) CMAKE_BUILD_TYPE="RelWithDebInfo" ;;
    *) echo "Error: MODE must be 'debug' or 'release', got: $MODE" >&2; exit 1 ;;
esac

case "$DEVICE" in
    qemu|qotom) ;;
    *) echo "Error: DEVICE must be 'qemu' or 'qotom', got: $DEVICE" >&2; exit 1 ;;
esac

# Paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/${DEVICE}/${MODE}"
DEVICE_DIR="${PROJECT_ROOT}/devices/${DEVICE}"
OUT_DIR="${PROJECT_ROOT}/artifacts"
ZIP_NAME="${MODE}_${DEVICE}.zip"

[[ -d "$DEVICE_DIR" ]] || { echo "Error: Missing device dir: $DEVICE_DIR" >&2; exit 1; }

# Configure and build
echo "Building: $MODE / $DEVICE"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE"
cmake --build "$BUILD_DIR" -j

# Run tests
if [[ "$RUN_TESTS" == "1" ]]; then
    echo "Running tests..."
    # --verbose
    ctest --test-dir "$BUILD_DIR" --output-on-failure || {
        echo "Warning: Tests failed but continuing build" >&2
    }
fi

# Package
BIN_PATH="${BUILD_DIR}/src/dpdk_forwarder"
[[ -f "$BIN_PATH" ]] || { echo "Error: Binary not found: $BIN_PATH" >&2; exit 1; }

STAGE_DIR="$(mktemp -d)"
trap 'rm -rf "$STAGE_DIR"' EXIT

cp -f "$BIN_PATH" "$STAGE_DIR/"
cp -f "${DEVICE_DIR}"/*.sh "$STAGE_DIR/" 2>/dev/null || {
    echo "Error: No .sh scripts in $DEVICE_DIR" >&2; exit 1;
}

mkdir -p "$OUT_DIR"
(cd "$STAGE_DIR" && zip -q -r "${OUT_DIR}/${ZIP_NAME}" .)

echo "✓ Created: ${OUT_DIR}/${ZIP_NAME}"
