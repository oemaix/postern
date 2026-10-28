FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        curl \
        git \
        python3 \
        sudo \
        xz-utils \
        zsh \
    && rm -rf /var/lib/apt/lists/*

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

RUN groupadd --gid "${USER_GID}" "${USERNAME}" \
    && useradd --uid "${USER_UID}" --gid "${USER_GID}" -m -s /bin/zsh "${USERNAME}" \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/${USERNAME}" \
    && chmod 0440 "/etc/sudoers.d/${USERNAME}"

USER ${USERNAME}
ENV PATH="/home/vscode/.nix-profile/bin:${PATH}" \
    USER="${USERNAME}"

RUN curl -fsSL https://nixos.org/nix/install -o /tmp/nix-install.sh \
    && sh /tmp/nix-install.sh --no-daemon --yes \
    && rm /tmp/nix-install.sh \
    && mkdir -p "/home/${USERNAME}/.config/nix" \
    && printf '%s\n' \
        'experimental-features = nix-command flakes' \
        'sandbox = false' \
        > "/home/${USERNAME}/.config/nix/nix.conf" \
    && nix_line='. "$HOME/.nix-profile/etc/profile.d/nix.sh"' \
    && printf '%s\n' "$nix_line" >> "/home/${USERNAME}/.bashrc" \
    && printf '%s\n' "$nix_line" >> "/home/${USERNAME}/.zshrc"

WORKDIR /workspaces/postern
