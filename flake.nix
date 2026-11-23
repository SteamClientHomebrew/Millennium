{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";

    # Locked bun commit - we need AT LEAST offline dependency resolution. Used by bun and bun2nix.
    nixpkgs-bun.url = "github:nixos/nixpkgs/619c39f6eac5d1ed6b8f073a9414baf7034b2356";
    
    # Used to get properly pinned dependencies that have a stable hash
    bun2nix = {
      url = "github:nix-community/bun2nix";
      inputs.nixpkgs.follows = "nixpkgs-bun";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      nixpkgs-bun,
      bun2nix,
      ...
    }:
    let 
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
      bunPkgs = nixpkgs-bun.legacyPackages.${system};
    in
    {
      packages.${system} = {
        default = self.packages.${system}.millennium-steam;
        
        millennium-steam = pkgs.callPackage ./packages/nix/steam.nix {
          inherit (self.packages.${system}) millennium;
        };

        millennium = pkgs.callPackage ./packages/nix/millennium.nix {
          inherit (bunPkgs) bun;
          src = self;
          bun2nix = bun2nix.packages.${system}.default;
        };
      };

      overlays.default = final: prev: {
        millennium = final.callPackage ./packages/nix/millennium.nix {
          inherit (nixpkgs-bun.legacyPackages.${final.stdenv.hostPlatform.system}) bun;
          src = self;
          bun2nix = bun2nix.packages.${system}.default;
        };
        millennium-steam = final.callPackage ./packages/nix/steam.nix { };
      };
    };
}
