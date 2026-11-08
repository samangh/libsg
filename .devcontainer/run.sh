#!/usr/bin/env bash
#
# Build (if needed) and enter the libsg development container.
#
#   ./.devcontainer/run.sh                  # interactive shell
#   ./.devcontainer/run.sh claude           # Claude Code, normal permissions
#   ./.devcontainer/run.sh auto             # Claude Code, --dangerously-skip-permissions
#   ./.devcontainer/run.sh <any command>    # run a one-off command
#
# Environment overrides: IMAGE, CPM_CACHE, REBUILD=1

set -euo pipefail

IMAGE=${IMAGE:-libsg-dev}
DEVCONTAINER_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if ! [[ $# -ge 1 ]]; then
	echo "Mounts the given path and opens in a docker image."
	echo 'Syntax: $0 path-to-libsg-folder <optional commands>'
    echo
	exit 1
fi

# Docker treats a bare name (e.g. `myproject`) as a named volume rather
# than a bind mount, so always pass an absolute path.
if [[ ! -d $1 ]]; then
    echo "Not a directory: $1"
    exit 1
fi
DIR="$(realpath "$1")"

# `id -g` reports the *current* primary group, which is the docker group when
# invoked under `newgrp docker` / `sg docker`. Look the group up by user name
# instead so the container user always matches your login group.
HOST_UID=$(id -u)
HOST_GID=$(id -g "$(id -un)")

if [[ -n ${REBUILD:-} ]] || ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    docker build \
        --build-arg "UID=$HOST_UID" \
        --build-arg "GID=$HOST_GID" \
        -t "$IMAGE" \
        "$DEVCONTAINER_DIR"
fi

case ${2:-} in
    auto) CMD=(claude --dangerously-skip-permissions) ;;
    "")   CMD=(/bin/bash) ;;
    *)    CMD=("${@:2}") ;;
esac

# Allocate a TTY only when we have one, so that non-interactive invocations
# (scripts, CI, `run.sh ctest ...` from a pipe) do not fail.
TTY_ARGS=(-i)
[[ -t 0 && -t 1 ]] && TTY_ARGS=(-i -t)

# TSan calls`personality(ADDR_NO_RANDOMIZE)`, which blocked by the default seccomp profile. Use `--cap-add=SYS_PTRACE`
# and `--security-opt seccomp=unconfined` to fix this.
exec docker run --rm "${TTY_ARGS[@]}" \
    --hostname libsg-dev \
    --volume "$DIR:/workspace" \
    --workdir /workspace \
    --volume libsg-claude-config:/home/dev/.claude \
    --cap-add=SYS_PTRACE \
    --security-opt seccomp=unconfined \
    --env GH_TOKEN="${GH_TOKEN_CLAUDE}" \
    "$IMAGE" \
    "${CMD[@]}"
