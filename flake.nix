{
  description = "Postern development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      eachSystem = nixpkgs.lib.genAttrs systems;
    in
    {
      devShells = eachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            # ESP-IDF is the framework (D-0007). nixpkgs 26.05 has esptool,
            # not a packaged IDF or Xtensa toolchain. Do not add a third-party
            # ESP-IDF flake here until it builds against this nixpkgs pin.
            packages = with pkgs; [
              bashInteractive
              zsh
              git
              python3
              uv
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
              esptool
            ];
          };
        }
      );
    };
}
