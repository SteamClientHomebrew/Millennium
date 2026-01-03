{
  cmake,
  gcc_multi,
  ninja,
  gtk3,
  pkg-config,
  git,
  lib,
  pkgsi686Linux,
  millennium-shims,
  millennium-assets,
  millennium-frontend,
  millennium-python,
  inputs,
  ...
}:
pkgsi686Linux.stdenv.mkDerivation (finalAttrs: {
  pname = "millennium";
  version = "2.34.0";

  src = inputs.millennium-src;

  nativeBuildInputs = [
    cmake
    gcc_multi
    ninja
    pkg-config
    git
  ];

  buildInputs = [
    pkgsi686Linux.gtk3
    pkgsi686Linux.libpsl
    pkgsi686Linux.openssl
    pkgsi686Linux.python311
    pkgsi686Linux.libxtst
    millennium-python
  ];

  cmakeGenerator = "Ninja";
  cmakeBuildType = "Release";
  enableParallelBuilding = true;

  cmakeFlags = [
    "-DGITHUB_ACTION_BUILD=ON"
    "-DDISTRO_NIX=ON"
    "-DNIX_FRONTEND=${millennium-frontend}/share/frontend"
    "-DNIX_SHIMS=${millennium-shims}/share/millennium/shims"
    "-DNIX_PYTHON=${millennium-python}"
    "--preset=linux-release"
  ];

  postPatch = ''
    mkdir -p deps

    prepare_dep() {
      local name="$1"
      local src="$2"
      echo "[Nix Millennium Build Setup] Preparing dependency: $name"
      cp -r --no-preserve=mode "$src" "deps/$name"
      chmod -R u+w "deps/$name"
    }

    echo "[Nix Millennium Build Setup] Copying all flake inputs to local writable directories"
    ${
      let
        deps = [
          "zlib"
          "luajit"
          "luajson"
          "minhook"
          "mini"
          "websocketpp"
          "fmt"
          "json"
          "libgit2"
          "minizip"
          "curl"
          "incbin"
          "asio"
          "abseil"
          "re2"
        ];
      in
      lib.concatStrings (map (dep: "prepare_dep ${dep} \"${inputs."${dep}-src"}\"\n") deps)
    }

    echo "[Nix Millennium Build Setup] Initializing Git Repos and adding Dummy Commits"
    echo "[Nix Millennium Build Setup] Dummy commits are used to determine versions, but flake inputs strip git history, causing issues"

    export HOME=$(pwd)

    git config --global init.defaultBranch main
    git config --global user.email "nix-build@localhost"
    git config --global user.name "Nix Build"

    git init
    git add .
    git commit -m "Dummy commit for build" > /dev/null 2>&1

    git init deps/luajit
    git -C deps/luajit add .
    git -C deps/luajit commit -m "Dummy Commit for Nix Build" > /dev/null 2>&1

    chmod -R u+rwx deps/
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/lib/millennium/

    install -Dm755  src/libmillennium_x86.so                        $out/lib/millennium/libmillennium_x86.so
    install -Dm755  src/boot/linux/libmillennium_bootstrap_86x.so   $out/lib/millennium/libmillennium_bootstrap_86x.so
    install -Dm755  src/hhx64/libmillennium_hhx64.so                $out/lib/millennium/libmillennium_hhx64.so

    runHook postInstall
  '';

})
