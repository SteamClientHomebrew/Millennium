{
  src,
  cmake,
  ninja,
  pkg-config,
  bun,
  bun2nix,
  git,
  nodejs,
  patchelf,
  rustPlatform,
  cargo,
  rustc,

  lib,
  stdenv,
  pkgsi686Linux,
  libx11,
  libXi,
  libXtst,
  re2,
  runCommand,

  openssl,
  curl,
  minizip-ng,
  bzip2,
  xz,
  zstd,
  nlohmann_json,
  zlib,
  ...
}:
let
  version = "3.6.3";

  tsPackages = [
    "src/typescript/ttc"
    "src/typescript/sdk"
    "src/typescript/frontend"
  ];
  
  tsBuildCmds = lib.concatMapStrings (p: "( cd ${p} && bun run build )\n") tsPackages;

  commonBuild = {
    inherit version src;
    
    /* 
      Generate bun dependency lock with:
      nix run github:nix-community/bun2nix -- \
        --lock-file src/typescript/bun.lock \
        --output-file packages/nix/bun.nix \
        --copy-prefix './../../src/typescript/'
    */
    bunDeps = bunDeps;
    bunRoot = "src/typescript";

    bunInstallFlags = [
      "--offline"
      "--frozen-lockfile"
    ];

    # We provide our own build phase.
    dontUseBunBuild = true;
    dontUseBunCheck = true;
    dontRunLifecycleScripts = true;

    nativeBuildInputs = [ 
      cmake
      ninja
      pkg-config
      git
      bun
      bun2nix.hook
      nodejs
      patchelf
    ];

    cmakeGenerator = "Ninja";
    cmakeBuildType = "Release";
    enableParallelBuilding = true;

    cmakeFlags = [
      "-DMILLENNIUM_BUILD_TESTS=OFF"
    ];

    preConfigure = ''
      export HOME="$TMPDIR"

      # Install Bun dependencies while still in the source directory.
      bunNodeModulesInstallPhase

      # Disable the automatically scheduled second installation.
      bunNodeModulesInstallPhase() {
        :
      }

      # Generate TTC's distribution.
      (
        cd src/typescript/ttc
        bun node_modules/.bin/rollup -c
      )

      # Build all TypeScript workspaces.
      ${tsBuildCmds}
    '';

    buildPhase = ''
      runHook preBuild
      cmake --build .
      runHook postBuild
    '';
  };

  bunDeps = bun2nix.fetchBunDeps {
    bunNix = ./bun.nix;
  };

  cargoDeps = rustPlatform.importCargoLock {
    lockFile = "${src}/src/instrumentation/loopback/Cargo.lock";
  };

  build32 = pkgsi686Linux.stdenv.mkDerivation (commonBuild // {
    pname = "millennium-32";

    buildInputs = [
      pkgsi686Linux.gtk3
      pkgsi686Linux.openssl
      pkgsi686Linux.libxtst
      pkgsi686Linux.curl
      pkgsi686Linux.minizip-ng
      pkgsi686Linux.bzip2
      pkgsi686Linux.xz
      pkgsi686Linux.zstd
      pkgsi686Linux.nlohmann_json
      pkgsi686Linux.zlib
    ];

    cmakeFlags = commonBuild.cmakeFlags ++ [ "-DMILLENNIUM_LINUX_X64=OFF" "-DDISTRO_NIX=ON" ];

    installPhase = ''
      runHook preInstall
      mkdir -p $out/lib/
      install -Dm755 libmillennium_x86.so           $out/lib/libmillennium_x86.so
      install -Dm755 libmillennium_luavm_x86        $out/lib/libmillennium_luavm_x86
      install -Dm755 libmillennium_bootstrap_x86.so $out/lib/libmillennium_bootstrap_x86.so
      runHook postInstall
    '';

    postFixup = ''
      patchelf --force-rpath --set-rpath "$(patchelf --print-rpath $out/lib/libmillennium_x86.so)" $out/lib/libmillennium_x86.so
      patchelf --force-rpath --set-rpath "$(patchelf --print-rpath $out/lib/libmillennium_bootstrap_x86.so)" $out/lib/libmillennium_bootstrap_x86.so
    '';
  });

  build64 = stdenv.mkDerivation (commonBuild // {
    pname = "millennium-64";

    nativeBuildInputs = commonBuild.nativeBuildInputs ++ [
      cargo
      rustc
    ];

    buildInputs = [
      libx11
      libXi
      libXtst
      re2
      re2.dev
      openssl
      curl
      minizip-ng
      bzip2
      xz
      zstd
      nlohmann_json
      zlib
    ];

    preConfigure = commonBuild.preConfigure + ''
      mkdir -p src/instrumentation/loopback/.cargo
      cat > src/instrumentation/loopback/.cargo/config.toml << 'EOF'
      [source.crates-io]
      replace-with = "vendored-sources"

      [source.vendored-sources]
      directory = "${cargoDeps}"
      EOF
    '';

    cmakeFlags = commonBuild.cmakeFlags ++ [ "-DMILLENNIUM_LINUX_X64=ON" "-DDISTRO_NIX=ON" ];

    installPhase = ''
      runHook preInstall
      mkdir -p $out/lib/
      install -Dm755 libmillennium_x64.so            $out/lib/libmillennium_x64.so
      install -Dm755 libmillennium_luavm_x64         $out/lib/libmillennium_luavm_x64
      install -Dm755 libmillennium_bootstrap_x64.so  $out/lib/libmillennium_bootstrap_x64.so
      install -Dm755 libmillennium_hhx64.so          $out/lib/libmillennium_hhx64.so
      install -Dm755 libmillennium_pvs64             $out/lib/libmillennium_pvs64
      install -Dm755 libmillennium_bootstrap_hhx64.so $out/lib/libmillennium_bootstrap_hhx64.so
      runHook postInstall
    '';

    postFixup = ''
      patchelf --force-rpath --set-rpath "$(patchelf --print-rpath $out/lib/libmillennium_x64.so)" $out/lib/libmillennium_x64.so
      patchelf --force-rpath --set-rpath "$(patchelf --print-rpath $out/lib/libmillennium_bootstrap_x64.so)" $out/lib/libmillennium_bootstrap_x64.so
    '';
  });
in
runCommand "millennium-${version}" {
  meta = {
    homepage = "https://steambrew.app/";
    license = lib.licenses.mit;
    description = "Modding framework to create, manage and use themes/plugins for Steam";
    longDescription = "An open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead";

    maintainers = [ lib.maintainers.trivaris ];
    platforms   = [ "x86_64-linux" ];
  };
} ''
  mkdir -p $out/lib
  cp ${build32}/lib/* $out/lib/
  cp ${build64}/lib/* $out/lib/
''
