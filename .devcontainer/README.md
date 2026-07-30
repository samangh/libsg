# Development container

A Debian trixie container with the full libsg toolchain, intended for running
builds, the test suite, and Claude Code in an isolated filesystem.

## One-off host setup

```sh
sudo apt install docker.io
sudo usermod -aG docker "$USER"   # log out and back in, or: newgrp docker
```

## Usage

```sh
./.devcontainer/run.sh          # interactive shell (builds the image first time)
./.devcontainer/run.sh auto     # Claude Code with --dangerously-skip-permissions
REBUILD=1 ./.devcontainer/run.sh   # force an image rebuild
```

The repository is bind-mounted at `/workspace` as UID/GID 1000, so anything the
container writes stays owned by you on the host. VS Code and CLion can use
`devcontainer.json` directly instead of `run.sh`.

## Building inside the container

Use a build directory that is distinct from the host ones — a CMake cache
records absolute compiler paths and cannot be shared between host and
container. `cmake-build-*` is already gitignored.

```sh
cmake -S . -B cmake-build-container \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DBUILD_TESTING=ON -DLIBSG_IMGUI=OFF
cmake --build cmake-build-container -j"$(nproc)"
ctest --test-dir cmake-build-container --output-on-failure
```

`CMAKE_GENERATOR=Ninja` and `CPM_SOURCE_CACHE` are already set in the
environment. Boost 1.88, fmt, libuv and zstd come from Debian packages, so
`OWN_BOOST` / `OWN_FMT` / `OWN_UV` are not needed.

## Notes

- **Sanitizers.** The container runs with `--cap-add=SYS_PTRACE` and
  `--security-opt seccomp=unconfined`, which TSan needs (it calls
  `personality(ADDR_NO_RANDOMIZE)`, blocked by the default seccomp profile).
  If an ASan build dies with *"Shadow memory range interleaves with an existing
  memory mapping"*, lower the host's ASLR entropy:
  `sudo sysctl vm.mmap_rnd_bits=28`.
- **ImGui** compiles (SDL2 and OpenGL headers are installed) but cannot be run;
  there is no display. Pass `-DLIBSG_IMGUI=OFF` to skip it.
- **Networking tests** only rely on loopback and resolving `localhost`, both of
  which work in the default bridge network.
- **Claude credentials** live on the `libsg-claude-config` named volume, not in
  your host `~/.claude`, so you authenticate once per volume rather than
  sharing host config with the container. `docker volume rm
  libsg-claude-config` to reset.
- **Isolation is filesystem-only.** The container has unrestricted network
  access, and `--dangerously-skip-permissions` does not stop code in the
  container from exfiltrating anything reachable from it — including the Claude
  credentials on that volume. Anthropic's reference devcontainer
  (<https://github.com/anthropics/claude-code/tree/main/.devcontainer>) adds an
  `init-firewall.sh` egress allowlist if you want that later; it needs
  `--cap-add=NET_ADMIN --cap-add=NET_RAW`.
