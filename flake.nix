{
  description = "";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    flake-parts = {
      url = "github:hercules-ci/flake-parts";
      inputs.nixpkgs-lib.follows = "nixpkgs";
    };
  };

  outputs = inputs @ { self, flake-parts, ...}:
    flake-parts.lib.mkFlake {inherit inputs;} {
      systems = ["x86_64-linux" "aarch64-linux"];

      perSystem = {
        pkgs,
        system,
        ...
      }: let
        xmrig-switch = pkgs.callPackage ./. {};
      in {
        formatter = pkgs.alejandra;

        packages = {
          default = xmrig-switch;
          inherit xmrig-switch;
        };
      };

      flake = {
        nixosModules = {
          xmrig-switch = import ./module.nix;
          default = self.nixosModules.xmrig-switch;
        };
      };
    };
}
