{
  description = "Postern development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  # Official EIM binaries, built with the pkgs passed in. Follow this pin
  # so the wrapper does not pull nixos-unstable (D-0022).
  inputs.espidf.url = "github:hnaderi/espidf-nix";
  inputs.espidf.inputs.nixpkgs.follows = "nixpkgs";

  outputs =
    { nixpkgs, espidf, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      eachSystem = nixpkgs.lib.genAttrs systems;
      # One tag for mkEspIdf and for the eim paths under ~/.espressif (D-0022).
      idfVersion = "v6.1";
    in
    {
      devShells = eachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          idf = espidf.lib.mkEspIdf {
            inherit pkgs idfVersion;
            idfTargets = "esp32";
          };
          # eim records the home that ran the install. A devcontainer
          # install says /root. On the host that tree is $HOME/.espressif
          # and /root/.espressif is not it (P-0031). These names sit ahead
          # of the real tools and bind the tree for that one command.
          idfRootBind = pkgs.writeShellScript "esp-idf-root-bind" ''
            name=''${0##*/}
            real=${idf}/bin/$name
            home_idf="$HOME/.espressif/${idfVersion}/esp-idf/tools/idf.py"
            baked_idf="/root/.espressif/${idfVersion}/esp-idf/tools/idf.py"
            if [ -f "$home_idf" ] && [ ! -f "$baked_idf" ]; then
              cache="''${XDG_CACHE_HOME:-$HOME/.cache}/espidf-nix/env-${idfVersion}"
              if [ -f "$cache" ] && grep -q '^ESP_IDF_VERSION=$' "$cache"; then
                rm -f "$cache"
              fi
              root_alias="''${XDG_RUNTIME_DIR:-$HOME/.cache}/postern-espressif-root"
              if mkdir -p "$root_alias" && ln -sfn "$HOME/.espressif" "$root_alias/.espressif"; then
                exec ${pkgs.bubblewrap}/bin/bwrap \
                  --dev-bind / / \
                  --bind "$root_alias" /root \
                  --die-with-parent \
                  -- "$real" "$@"
              fi
            fi
            exec "$real" "$@"
          '';
          idfBins = pkgs.linkFarm "esp-idf-root-bind" (
            map (name: {
              name = "bin/${name}";
              path = idfRootBind;
            }) [
              "idf.py"
              "idf-install"
              "esptool.py"
              "espefuse.py"
              "espsecure.py"
              "esp-coredump"
              "openocd"
              "eim"
            ]
          );
        in
        {
          default = pkgs.mkShell {
            # ESP-IDF comes from hnaderi/espidf-nix at idfVersion (D-0022).
            # That wrapper uses Python 3.13 from this nixpkgs pin. EIM
            # installs the chip toolchain into ~/.espressif on the first
            # idf.py run. This flake does not carry a project Python (D-0019).
            # shell/.zshenv skips the host global zshrc, and shell/.zshrc
            # is the interactive prompt (P-0022). gcc checks the register file.
            shellHook = ''
              export SHELL=${pkgs.zsh}/bin/zsh
              export PATH="${idfBins}/bin''${PATH:+:$PATH}"
              zdotdir=${./shell}
              if [ -n "''${ZDOTDIR:-}" ] && [ -f "''${ZDOTDIR}/.zshrc" ] && [ "''${ZDOTDIR}" != "$zdotdir" ]; then
                export DEV_SAVED_ZDOTDIR="''${ZDOTDIR}"
              fi
              export ZDOTDIR="$zdotdir"
            '';
            packages =
              (with pkgs; [
                bashInteractive
                zsh
                git
                curl
                cacert
                coreutils
                findutils
                gnugrep
                gnused
                gawk
                which
                cmake
                ninja
                gcc
              ])
              ++ [
                idf
              ];
          };
        }
      );
    };
}
