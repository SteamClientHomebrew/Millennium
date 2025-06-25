{
  steam,
  pkgsi686Linux,
  replaceVars,
  cmake,
  ninja,
  callPackage,
  lib,
}:
let
  shims = callPackage ./typescript/shims.nix { };
  assets = callPackage ./assets.nix { };
  
in
pkgsi686Linux.stdenv.mkDerivation {
  pname = "millennium";
  version = "git";

  src = ../.;
  patches = [
    ./patches/disable-cli.patch
    (replaceVars ./patches/start-script.patch {
      inherit steam;
      OUT = null;
    })
    (replaceVars ./patches/fix-paths.patch {
      inherit shims assets;
      OUT = null;
    })
    ./patches/cmake.patch
  ];

  buildInputs = [
    shims
    assets
    pkgsi686Linux.python311
    (pkgsi686Linux.openssl.override {
      static = true;
    })
    (
      (pkgsi686Linux.curl.override {
        #TODO: what the actual fuck is happening here
        #      why does nix set every attribute to 'true' ?????
        http2Support = false; # ;
        gssSupport = false; # ;
        zlibSupport = true;
        opensslSupport = true;
        brotliSupport = false;
        zstdSupport = false;
        http3Support = false;
        scpSupport = false;
        pslSupport = false;
        idnSupport = false;
      }).overrideAttrs
      (old: {
        configureFlags = (old.configureFlags or [ ]) ++ [
          "--enable-static"
          "--disable-shared"
        ];
        propagatedBuildInputs = [
          (pkgsi686Linux.openssl.override {
            static = true;
          }).out
        ];
      })
    )
  ];
  nativeBuildInputs = [
    cmake
    ninja
  ];
  configurePhase = ''
    cmake -G Ninja
    substituteInPlace scripts/posix/start.sh \
      --replace-fail '@OUT@' "$out"
    substituteInPlace src/sys/env.cc \
      --replace-fail '@OUT@' "$out"
  '';
  buildPhase = ''
    cmake --build . --config Release -- -j$(nproc)
  '';
  installPhase = ''
    mkdir -p $out/bin $out/lib/millennium
    cp libmillennium_x86.so $out/lib/millennium
    cp scripts/posix/start.sh $out/bin/millennium
  '';
  NIX_CFLAGS_COMPILE = [
    "-isystem ${pkgsi686Linux.python311}/include/${pkgsi686Linux.python311.libPrefix}"
  ];
  NIX_LDFLAGS = [ "-l${pkgsi686Linux.python311.libPrefix}" ];

  meta = with lib; {
    maintainers = with maintainers; [ Sk7Str1p3 ];
  };
}
