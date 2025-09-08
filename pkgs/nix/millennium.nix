{
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
  venv = pkgsi686Linux.python311.withPackages (
    py:
    (with py; [
      setuptools
      pip

      arrow
      psutil
      requests
      gitpython
      cssutils
      websockets
      watchdog
      pysocks
      pyperclip
      semver
    ])
    ++ [
      (callPackage ./python/millennium.nix)
      (callPackage ./python/core-utils.nix)
    ]
  );
in
pkgsi686Linux.stdenv.mkDerivation {
  pname = "millennium";
  version = "git";

  src = ../.;

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
  env = {
    NIX_OS = 1;
    inherit venv assets shims;
  };
  installPhase = ''
    runHook preInstall

    mkdir -p $out/lib/millennium
    cp libmillennium_x86.so $out/lib/millennium
    
    runHook postInstall
  '';
  NIX_CFLAGS_COMPILE = [
    "-isystem ${pkgsi686Linux.python311}/include/${pkgsi686Linux.python311.libPrefix}"
  ];
  NIX_LDFLAGS = [ "-l${pkgsi686Linux.python311.libPrefix}" ];

  meta = with lib; {
    maintainers = with maintainers; [ Sk7Str1p3 ];
  };
}
