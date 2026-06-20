{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

    millennium-src.url   = "github:SteamClientHomebrew/Millennium/bd79fe7a94aad1e840d8cae54d6b5233d80268a8";
    millennium-src.flake = false;

  };

  outputs =
    {
      self,
      nixpkgs,
      millennium-src,
      ...
    }@inputs:
    {
      packages.x86_64-linux =
        let
          pkgs = import nixpkgs {
            system = "x86_64-linux";
            config.allowUnfree = true;
          };

          packages = {
            default          = packages.millennium-steam;
            millennium       = pkgs.callPackage ./millennium.nix { inherit millennium-src; };
            millennium-steam = pkgs.callPackage ./steam.nix {
              inherit (packages) millennium;
            };
          };
        in
        packages;

      overlays.default = final: prev: {
        inherit (self.packages.${prev.stdenv.hostPlatform.system}) millennium-steam;
      };
    };
}
