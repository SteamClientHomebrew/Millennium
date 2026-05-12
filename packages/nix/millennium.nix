{
  lib,
  stdenv,
  callPackage,
  fetchFromGitHub,

  inputs,
  millennium-shims,
  millennium-assets,
  millennium-frontend,
  millennium-32,
  millennium-64,
  pkgsi686Linux,
  cacert,
  millennium-python ? pkgsi686Linux.python311,
  ...
}:
stdenv.mkDerivation {
  pname = "millennium";
  version = "2.36.0";

  phases = [
    "installPhase"
    "fixupPhase"
  ];

  buildInputs = [
    pkgsi686Linux.gtk3
    pkgsi686Linux.libpsl
    pkgsi686Linux.openssl
    pkgsi686Linux.libxtst
    cacert
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
    "-DCURL_CA_BUNDLE=${cacert}/etc/ssl/certs/ca-bundle.crt"
    "-DCURL_CA_PATH=${cacert}/etc/ssl/certs"
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
          "websocketpp"
          "json"
          "minizip"
          "curl"
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
    mkdir -p $out/lib

    echo "[Nix Millennium] Merging 32-bit libraries..."
    cp -v ${millennium-32}/lib/*.so $out/lib/

    echo "[Nix Millennium] Merging 64-bit libraries..."
    cp -v ${millennium-64}/lib/*.so $out/lib/
  '';

  passthru = {
    assets = millennium-assets;
    shims = millennium-shims;
    frontend = millennium-frontend;
  };

  meta = {
    homepage = "https://steambrew.app/";
    license = lib.licenses.mit;
    description = "Modding framework to create, manage and use themes/plugins for Steam";

    longDescription = "An open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead";

    maintainers = [
      lib.maintainers.trivaris
    ];

    platforms = [
      "x86_64-linux"
    ];
  };
}
