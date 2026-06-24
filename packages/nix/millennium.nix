{
  cmake,
  ninja,
  pkg-config,
  bun,
  nodejs,
  cacert,
  patchelf,

  lib,
  stdenv,
  pkgsi686Linux,
  libx11,
  libXi,
  libXtst,
  re2,
  runCommand,

  millennium-src,
  ...
}:
let
  version = "3.3.1";

  tsPackages = [
    "src/typescript/ttc"
    "src/typescript/sdk/packages/client"
    "src/typescript/sdk/packages/browser"
    "src/typescript/sdk/packages/loader"
    "src/typescript/frontend"
  ];

  tsDirs = lib.concatStringsSep " " (
    [ "src/typescript/node_modules" ] ++ map (p: "${p}/node_modules") tsPackages
  );
  tsBuildCmds = lib.concatMapStrings (p: "( cd ${p} && bun run build )\n") tsPackages;

  commonBuild = {
    inherit version;
    src = millennium-src;

    nativeBuildInputs = [ cmake ninja pkg-config ];

    cmakeGenerator      = "Ninja";
    cmakeBuildType      = "Release";
    enableParallelBuilding = true;

    cmakeFlags = [
      "-DMILLENNIUM_BUILD_TESTS=OFF"
    ];

    buildPhase = ''
      runHook preBuild
      cmake --build .
      runHook postBuild
    '';
  };

  bunDeps = stdenv.mkDerivation {
    name = "millennium-typescript-bun-deps";

    src = millennium-src;
    nativeBuildInputs = [ bun cacert ];
    SSL_CERT_FILE = "${cacert}/etc/ssl/certs/ca-bundle.crt";

    buildPhase = ''
      export HOME=$TMPDIR
      (cd src/typescript && bun install --frozen-lockfile)
    '';

    installPhase = ''
      mkdir -p $out

      for dir in ${tsDirs}; do
        if [ -d "$dir" ]; then
          mkdir -p "$out/$(dirname "$dir")"
          cp -r "$dir" "$out/$dir"
        fi
      done
    '';

    outputHashMode = "recursive";
    outputHashAlgo = "sha256";
    outputHash = "sha256-07kSANQ5uLwsLX+wuS0Cq2YSeTqkSVnUBGqadP0Uj4Y=";
  };

  build32 = pkgsi686Linux.stdenv.mkDerivation (commonBuild // {
    pname = "millennium-32";

    nativeBuildInputs = commonBuild.nativeBuildInputs ++ [ bun nodejs patchelf ];

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

    cmakeFlags = commonBuild.cmakeFlags ++ [ "-DDISTRO_NIX=ON" ];

    postPatch = ''
      export HOME=$TMPDIR

      for dir in ${tsDirs}; do
        if [ -d ${bunDeps}/$dir ]; then
          mkdir -p "$(dirname "$dir")"
          cp -r ${bunDeps}/$dir "$dir"
          chmod -R u+w "$dir"
        fi
      done

      patchShebangs src/typescript/node_modules
      ${tsBuildCmds}
    '';

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

    buildInputs = [
      libx11
      libXi
      libXtst
      re2
      re2.dev
    ];

    cmakeFlags = commonBuild.cmakeFlags ++ [ "-DMILLENNIUM_NIX_64BIT=ON" ];

    installPhase = ''
      runHook preInstall
      mkdir -p $out/lib/
      install -Dm755 libmillennium_hhx64.so           $out/lib/libmillennium_hhx64.so
      install -Dm755 libmillennium_pvs64              $out/lib/libmillennium_pvs64
      install -Dm755 libmillennium_bootstrap_hhx64.so $out/lib/libmillennium_bootstrap_hhx64.so
      runHook postInstall
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
