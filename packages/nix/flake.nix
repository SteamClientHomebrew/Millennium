{
  description = "Nix Build for Millennium";

  inputs = {
    # Bun FOD is sensitive to version changes, so we use a specific commit instead of a channel.
    nixpkgs.url = "github:nixos/nixpkgs/567a49d1913ce81ac6e9582e3553dd90a955875f";

    millennium-src.url   = "github:SteamClientHomebrew/Millennium/d6aed733f9b2e55eca2cb973a0bef8d9e05c5fca";
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
