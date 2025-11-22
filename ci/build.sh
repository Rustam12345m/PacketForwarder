#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-dpdk-forwarder-build}"

# Detect container engine
engine=""
if command -v docker >/dev/null 2>&1; then
    engine="docker"
elif command -v podman >/dev/null 2>&1; then
    engine="podman"
else
    echo "Error: Need docker or podman" >&2
    exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$engine" build -t "${IMAGE_NAME}" -f "${ROOT}/ci/Dockerfile" "${ROOT}"

# Interactive shell mode
if [[ "${1:-}" == "--shell" ]]; then
    exec "${ROOT}/ci/run_container.sh"
fi

# Configure user/permissions
run_user_args=()
if [[ "$engine" == "docker" ]]; then
    run_user_args+=(--user "$(id -u):$(id -g)" -e HOME=/tmp)
else
    run_user_args+=(--userns=keep-id)
fi

# Pass all arguments through to build script
exec "$engine" run --rm \
    "${run_user_args[@]}" \
    -v "${ROOT}:/work" \
    -w /work \
    "${IMAGE_NAME}" \
    bash -lc "./ci/build_inside_container.sh $*"

