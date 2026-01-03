{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    millennium-src.url = "github:SteamClientHomebrew/millennium/91bd5b530859db93857c3b0908e81809b5f3907e";
    millennium-src.flake = false;
  };

  outputs =
    {
      self,
      nixpkgs,
      millennium-src,
    }:
    {
      packages.x86_64-linux =
        let
          pkgs = import nixpkgs { system = "x86_64-linux"; };
        in
        {
          millennium-assets   = pkgs.callPackage ./assets.nix { inherit millennium-src; };
          millennium-frontend = pkgs.callPackage ./frontend.nix { inherit millennium-src; };
        };
    };
}
