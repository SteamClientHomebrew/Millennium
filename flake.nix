{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";
    
    # Used to get properly pinned dependencies that would have a stable hash.
    /* 
      Generate with:
      nix run github:nix-community/bun2nix -- \
        --lock-file src/typescript/bun.lock \
        --output-file packages/nix/bun.nix \
        --copy-prefix ./../../src/typescript/
    */
    bun2nix = {
      url = "github:nix-community/bun2nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      bun2nix,
      ...
    }:
    let 
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
    in
    {
      packages.${system} = {
        default = self.packages.${system}.millennium-steam;
        
        millennium-steam = pkgs.callPackage ./packages/nix/steam.nix {
          inherit (self.packages.${system}) millennium;
        };

        millennium = pkgs.callPackage ./packages/nix/millennium.nix { 
          bun2nix = bun2nix.packages.${pkgs.stdenv.hostPlatform.system}.default;
        };
      };

      overlays.default = final: prev: {
        millennium = final.callPackage ./packages/nix/millennium.nix {
          bun2nix = bun2nix.packages.${final.stdenv.hostPlatform.system}.default;
        };
        millennium-steam = final.callPackage ./packages/nix/steam.nix { };
      };
    };
}
