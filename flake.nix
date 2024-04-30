{
  description = "Qemu";

  inputs = {
    flake-utils.url = "github:numtide/flake-utils";
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, flake-utils, nixpkgs }:
    flake-utils.lib.eachSystem [ "x86_64-linux" ] (system:
      let
        pkgs = import nixpkgs { inherit system; };
        qemu = pkgs.callPackage ./qemu.nix { };
        #qemu = pkgs.qemu;
      in rec {
        packages = { default = qemu; };
        devShells.default = pkgs.mkShell {
          name = "${qemu.name}-dev";
          buildInputs = [ qemu ];
          inputsFrom = [ qemu ];
          nativeBuildInputsFrom = [ qemu ];
        };
      });
}
