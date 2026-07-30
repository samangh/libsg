FROM debian:trixie

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
      build-essential clang llvm lld gdb cmake pkg-config \
      binutils git ca-certificates curl \
      libboost1.88-all-dev libfmt-dev libuv1-dev libzstd-dev \
      libsdl2-dev libgl-dev libglu1-mesa-dev \
      doxygen graphviz \
      sudo less procps ripgrep ccache
RUN rm -rf /var/lib/apt/lists/*

# A non-root user is not optional: `claude --dangerously-skip-permissions`
# refuses to run as root. UID/GID default to 1000 so that files created in the
# bind-mounted worktree stay owned by you on the host.
ARG USERNAME=dev
ARG UID=1000
ARG GID=1000

RUN groupadd --gid ${GID} ${USERNAME} \
    && useradd --uid ${UID} --gid ${GID} --create-home --shell /bin/bash ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

# Kept on a named volume so authentication survives container recreation.
ENV CLAUDE_CONFIG_DIR=/home/${USERNAME}/.claude
ENV PATH=/home/${USERNAME}/.local/bin:${PATH}

RUN install -d -o ${UID} -g ${GID} /workspace

USER ${USERNAME}
WORKDIR /workspace

RUN mkdir -p /home/${USERNAME}/.claude \
             /home/${USERNAME}/.local/bin

# Native installer; lands in ~/.local/bin/claude with the versions under
# ~/.local/share/claude. `claude update` works inside a running container.
RUN curl -fsSL https://claude.ai/install.sh | bash

CMD ["/bin/bash"]