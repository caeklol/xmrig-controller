# Use `nix run .`
{
  description = "XMRig Switch Flake for development";
  inputs.nixpkgs.url = "nixpkgs/nixos-23.11";

  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin"];
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    nixpkgsFor = forAllSystems (system:
      import nixpkgs {
        inherit system;
        overlays = [self.overlay];
      });
  in {
    formatter.x86_64-linux = nixpkgs.legacyPackages.x86_64-linux.alejandra;
    overlay = final: prev: {
      xmrig-switch = with final;
        stdenv.mkDerivation rec {
          pname = "xmrig-switch";
          version = "3";

          src = ./.;
          buildInputs = with pkgs; [
            systemdLibs
			libudev-zero
            pkg-config
          ];

          nativeBuildInputs = [autoreconfHook];
        };
    };
    packages = forAllSystems (system: {
      inherit (nixpkgsFor.${system}) xmrig-switch;
    });

    defaultPackage = forAllSystems (system: self.packages.${system}.xmrig-switch);
  };
}
