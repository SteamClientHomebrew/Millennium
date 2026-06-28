{
  description = "Nix Build for Millennium";

  inputs = {
    # Bun FOD is sensitive to version changes, so we use a specific commit instead of a channel.
    nixpkgs.url = "github:nixos/nixpkgs/567a49d1913ce81ac6e9582e3553dd90a955875f";

    millennium-src.url   = "github:SteamClientHomebrew/Millennium/5729ba4bb432a071a0276abb416b2c26d27afa71";
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
