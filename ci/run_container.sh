#!/bin/bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-dpdk-forwarder-build}"

engine=""
if command -v docker >/dev/null 2>&1; then
    engine="docker"
elif command -v podman >/dev/null 2>&1; then
    engine="podman"
else
    echo "Need docker or podman" >&2
    exit 1
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"$engine" build -t "$IMAGE_NAME" -f "${ROOT}/ci/Dockerfile" "$ROOT"

# Permissions: match host uid/gid so the mounted workspace stays writable.
run_user_args=()
if [[ "$engine" == "docker" ]]; then
    run_user_args+=(--user "$(id -u):$(id -g)" -e HOME=/tmp)
else
    run_user_args+=(--userns=keep-id)
fi

exec "$engine" run -it --rm \
    "${run_user_args[@]}" \
    -v "${ROOT}:/work" \
    -w /work \
    "$IMAGE_NAME" \
    /bin/bash

