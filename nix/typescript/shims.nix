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
    hash = "sha256-1cqsNIQnrVAe18FC8D6mnptH+gp4B5+hyGF9lGn0OE0=";
    fetcherVersion = 2;
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
