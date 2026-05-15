{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

    millennium-src.url   = "github:SteamClientHomebrew/Millennium/233f417dd99b3ece34d15b75e7cf5fd9d4487f15";
    millennium-src.flake = false;

    luajit-src.url   = "github:SteamClientHomebrew/LuaJIT/v2.1";
    luajit-src.flake = false;
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
            millennium       = pkgs.callPackage ./millennium.nix { inherit inputs millennium-src; };
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
