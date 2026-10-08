{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "https://channels.nixos.org/nixpkgs-unstable/nixexprs.tar.zst";
    
    # Only used for the bun FOD, whose hash depends on the exact bun version.
    # Do not make this follow another nixpkgs.
    nixpkgs-bun.url = "github:nixos/nixpkgs/c59305bab2065cfecc4944690d9eedbb56f3a9fa";

    millennium-src.url   = "github:SteamClientHomebrew/Millennium/765aa8802f8a4d942ad8ac8323a9e0f233a50fa8";
    millennium-src.flake = false;
  };

  outputs =
    {
      self,
      nixpkgs,
      nixpkgs-bun,
      millennium-src,
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
        
        millennium-steam = pkgs.callPackage ./steam.nix {
          inherit (self.packages.${system}) millennium;
        };

        millennium = pkgs.callPackage ./millennium.nix { 
          inherit millennium-src; 
          inherit (bunPkgs) bun;
        };
      };

      overlays.default = final: prev: {
        millennium = final.callPackage ./millennium.nix {
          inherit millennium-src;
          inherit (nixpkgs-bun.legacyPackages.${final.stdenv.hostPlatform.system}) bun;
        };
        millennium-steam = final.callPackage ./steam.nix { };
      };
    };
}
