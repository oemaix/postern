# NixOS still sources /etc/zshrc for an interactive shell. That file
# installs command_not_found_handler, which returns 127. The devcontainer
# image has no such rc. Skip global rc files so `exit` is not that status.
# /etc/zshenv still runs (P-0022).
unsetopt GLOBAL_RCS
