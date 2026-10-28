FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive

# The Nix installer runs before any devshell exists. sudo is for the
# entrypoint. git, Python, zsh, and bash in agent commands come from
# the flake.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
        sudo \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000

# ubuntu:26.04 already has user and group ubuntu at 1000:1000 (P-0021).
RUN if getent group "${USER_GID}" >/dev/null; then \
        existing_group="$(getent group "${USER_GID}" | cut -d: -f1)"; \
        if [ "${existing_group}" != "${USERNAME}" ]; then \
            groupmod --new-name "${USERNAME}" "${existing_group}"; \
        fi; \
    else \
        groupadd --gid "${USER_GID}" "${USERNAME}"; \
    fi \
    && if getent passwd "${USER_UID}" >/dev/null; then \
        existing_user="$(getent passwd "${USER_UID}" | cut -d: -f1)"; \
        if [ "${existing_user}" != "${USERNAME}" ]; then \
            usermod --login "${USERNAME}" --home "/home/${USERNAME}" --move-home "${existing_user}"; \
        fi; \
        usermod --gid "${USER_GID}" --shell /bin/bash "${USERNAME}"; \
    else \
        useradd --uid "${USER_UID}" --gid "${USER_GID}" -m -s /bin/bash "${USERNAME}"; \
    fi \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > "/etc/sudoers.d/${USERNAME}" \
    && chmod 0440 "/etc/sudoers.d/${USERNAME}"

USER ${USERNAME}
ENV PATH="/home/vscode/.nix-profile/bin:${PATH}" \
    USER="${USERNAME}"

# Nix 2.35.4. The floating installer URL tracks the current script,
# so a later rebuild would change this version (P-0026).
ARG NIX_VERSION=2.35.4
RUN curl -fsSL "https://releases.nixos.org/nix/nix-${NIX_VERSION}/install" -o /tmp/nix-install.sh \
    && sh /tmp/nix-install.sh --no-daemon --yes \
    && rm /tmp/nix-install.sh \
    && mkdir -p "/home/${USERNAME}/.config/nix" \
    && printf '%s\n' \
        'experimental-features = nix-command flakes' \
        'accept-flake-config = false' \
        'sandbox = false' \
        > "/home/${USERNAME}/.config/nix/nix.conf" \
    && nix_line='. "$HOME/.nix-profile/etc/profile.d/nix.sh"' \
    && printf '%s\n' "$nix_line" >> "/home/${USERNAME}/.bashrc"

# The entrypoint runs as root so it can move vscode onto the workspace
# owner. remoteUser in devcontainer.json is still vscode (P-0023).
# /etc/nix/nix.conf is read for every home, including /root after the
# uid move. The NixOS config bind-mounts the host /nix read-only over
# this image store (P-0024). The Ubuntu config does not (P-0025).
USER root
RUN mkdir -p /etc/nix /usr/local/bin \
 && printf '%s\n' \
      'experimental-features = nix-command flakes' \
      'accept-flake-config = false' \
      'sandbox = false' \
      > /etc/nix/nix.conf
COPY entrypoint.sh /usr/local/bin/devcontainer-entrypoint
COPY nix.sh /usr/local/bin/nix
RUN chmod 755 /usr/local/bin/devcontainer-entrypoint /usr/local/bin/nix
ENV PATH="/usr/local/bin:${PATH}"
ENTRYPOINT ["/usr/local/bin/devcontainer-entrypoint"]
CMD ["sleep", "infinity"]

WORKDIR /workspaces
