# Working directory, then # as root and % otherwise.
# This file is here so zsh/newuser does not return 1 for uid 0 (P-0022).
PROMPT='%~ %# '
if [[ -n ${DEV_SAVED_ZDOTDIR:-} && -r ${DEV_SAVED_ZDOTDIR}/.zshrc ]]; then
  source "${DEV_SAVED_ZDOTDIR}/.zshrc"
fi
true
