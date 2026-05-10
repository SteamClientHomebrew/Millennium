{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

    millennium-src.url = "github:SteamClientHomebrew/Millennium/1bc62c94a06f25f7e8d7e269f11cd968cf576bff";
    millennium-src.flake = false;

    zlib-src.url = "github:zlib-ng/zlib-ng/2.2.5";
    luajit-src.url = "github:SteamClientHomebrew/LuaJIT/v2.1";
    websocketpp-src.url = "github:zaphoyd/websocketpp/0.8.2";
    json-src.url = "github:nlohmann/json/v3.12.0";
    minizip-src.url = "github:zlib-ng/minizip-ng/4.0.10";
    curl-src.url = "github:curl/curl/curl-8_13_0";
    asio-src.url = "github:chriskohlhoff/asio/asio-1-30-0";

    abseil-src.url = "github:abseil/abseil-cpp/20240722.0";
    re2-src.url = "github:google/re2/2025-11-05";

    zlib-src.flake = false;
    luajit-src.flake = false;
    websocketpp-src.flake = false;
    json-src.flake = false;
    minizip-src.flake = false;
    curl-src.flake = false;
    asio-src.flake = false;

    abseil-src.flake = false;
    re2-src.flake = false;
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

          millennium-deps = {
            inherit inputs;
            inherit (packages)
              millennium-shims
              millennium-assets
              millennium-frontend
              ;
          };

          packages = {
            default             = packages.millennium-steam;
            millennium-assets   = pkgs.callPackage ./assets.nix         { inherit millennium-src; };
            millennium-frontend = pkgs.callPackage ./frontend.nix       { inherit millennium-src; };
            millennium-shims    = pkgs.callPackage ./shims.nix          { inherit millennium-src; };
            millennium-32       = pkgs.callPackage ./millennium-32.nix  ( millennium-deps );
            millennium-64       = pkgs.callPackage ./millennium-64.nix  ( millennium-deps );
            millennium          = pkgs.callPackage ./millennium.nix     ( millennium-deps // { inherit (packages) millennium-32 millennium-64; } );
            millennium-steam    = pkgs.callPackage ./steam.nix          { inherit (packages) millennium millennium-shims millennium-assets; };
          };
        in
        packages;

      overlays.default = final: prev: {
        inherit (self.packages.${prev.stdenv.hostPlatform.system}) millennium-steam;
      };
    };
}
