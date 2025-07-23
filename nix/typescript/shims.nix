{
  stdenv,
  nodejs,
  pnpm,
  faketty,
}:
stdenv.mkDerivation rec {
  pname = "millennium-sdk";
  version = "git";

  src = ../../sdk;
  pnpmDeps = pnpm.fetchDeps {
    inherit src version pname;
    #TODO: automatic hash update
    hash = "sha256-M2kvId8f1kV8gHSJWubNAS3u0IFrHWKfeci2zrj5lmQ=";
    fetcherVersion = 9;
  };

  nativeBuildInputs = [
    pnpm.configHook
    nodejs
    faketty
  ];

  buildPhase = ''
    runHook preBuild
    faketty pnpm run build
  '';

  installPhase = ''
    mkdir -p $out/share/millennium/shims
    cp -r typescript-packages/loader/build/* $out/share/millennium/shims
  '';
}
