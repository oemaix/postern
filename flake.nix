{
  description = "Postern development environment";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  # EIM binaries, built with this nixpkgs pin.
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
    in
    {
      devShells = eachSystem (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          # v6.1, esp32 only. The first idf.py run installs that target
          # under ~/.espressif. The toolchain stays out of the Nix store.
          idf = espidf.lib.mkEspIdf {
            inherit pkgs;
            idfVersion = "v6.1";
            idfTargets = "esp32";
          };
        in
        {
          # No shellHook. idf.py is on PATH through the package. Nothing
          # here rewrites HOME, ZDOTDIR, or /root.
          default = pkgs.mkShell {
            packages = [
              pkgs.git
              idf
            ];
          };
        }
      );
    };
}
