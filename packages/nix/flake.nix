{
  description = "Nix Build for Millennium";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";

    millennium-src.url = "github:SteamClientHomebrew/millennium/91bd5b530859db93857c3b0908e81809b5f3907e";
    millennium-src.flake = false;

    zlib-src.url = "github:zlib-ng/zlib-ng/2.2.5?shallow=true";
    luajit-src.url = "github:SteamClientHomebrew/LuaJIT/v2.1?shallow=true";
    luajson-src.url = "github:SteamClientHomebrew/LuaJSON/master?shallow=true";
    minhook-src.url = "github:TsudaKageyu/minhook/v1.3.4?shallow=true";
    mini-src.url = "github:metayeti/mINI/0.9.18?shallow=true";
    websocketpp-src.url = "github:zaphoyd/websocketpp/0.8.2?shallow=true";
    fmt-src.url = "github:fmtlib/fmt/12.0.0?shallow=true";
    json-src.url = "github:nlohmann/json/v3.12.0?shallow=true";
    libgit2-src.url = "github:libgit2/libgit2/v1.9.1?shallow=true";
    minizip-src.url = "github:zlib-ng/minizip-ng/4.0.10?shallow=true";
    curl-src.url = "github:curl/curl/curl-8_13_0?shallow=true";
    incbin-src.url = "github:graphitemaster/incbin/main?shallow=true";
    asio-src.url = "github:chriskohlhoff/asio/asio-1-30-0?shallow=true";

    abseil-src.url = "github:abseil/abseil-cpp/20240722.0";
    re2-src.url = "github:google/re2/2025-11-05";

    zlib-src.flake = false;
    luajit-src.flake = false;
    luajson-src.flake = false;
    minhook-src.flake = false;
    mini-src.flake = false;
    websocketpp-src.flake = false;
    fmt-src.flake = false;
    json-src.flake = false;
    libgit2-src.flake = false;
    minizip-src.flake = false;
    curl-src.flake = false;
    incbin-src.flake = false;
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
          pkgs = import nixpkgs { system = "x86_64-linux"; };
          packages = {
            millennium-assets = pkgs.callPackage ./assets.nix { inherit millennium-src; };
            millennium-frontend = pkgs.callPackage ./frontend.nix { inherit millennium-src; };
            millennium-shims = pkgs.callPackage ./shims.nix { inherit millennium-src; };
            millennium-python = pkgs.callPackage ./python.nix { };
            millennium = pkgs.callPackage ./millennium.nix {
              inherit (packages) millennium-assets millennium-frontend millennium-shims millennium-python;
              inherit inputs self;
            };
          };
        in
        packages;
    };
}
