# Special thanks to @Sk7Str1p3, @mourogurt, @kaeeraa, @mctrxw for help with this flake and packages
{
  description = ''
    Millennium - an open-source low-code modding framework to create, 
    manage and use themes/plugins for the desktop Steam Client
  '';

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    self.submodules = true; # Requires *Nix* >= 2.27
    flake-parts.url = "github:hercules-ci/flake-parts";
  };

  outputs =
    { nixpkgs, self, ... }:
    let
      pkgs = import nixpkgs {
        system = "x86_64-linux";
        config.allowUnfree = true;
      };
    in
    {
      overlays.default = final: prev: rec {
        inherit (self.packages."x86_64-linux") millennium;
        steam-millennium = final.steam.override (prev: {
          extraProfile =
            ''
              export LD_LIBRARY_PATH="${millennium}/lib/millenium/''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              export LD_PRELOAD="${millennium}/lib/millennium/libmillennium_x86.so''${LD_PRELOAD:+:$LD_PRELOAD}"
            ''
            + (prev.extraProfile or "");
        });
      };

      devShells."x86_64-linux".default = import ./shell.nix { inherit pkgs; };

      packages."x86_64-linux" = {
        default = self.packages."x86_64-linux".millennium;
        millennium = pkgs.callPackage ./nix/millennium.nix { };
        shims = pkgs.callPackage ./nix/typescript/shims.nix { };
        assets = pkgs.callPackage ./nix/assets.nix { };
        python = {
          millennium = pkgs.callPackage ./nix/python/millennium.nix { };
          core-utils = pkgs.callPackage ./nix/python/core-utils.nix { };
        };
      };
    };
}
