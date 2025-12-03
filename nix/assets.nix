{
  stdenv,
  pnpm,
  nodejs,
}:
stdenv.mkDerivation rec {
  pname = "millennium-assets";
  version = "git";

  src = ../assets;
  pnpmDeps = pnpm.fetchDeps {
    inherit src version pname;
    hash = "sha256-/FR6hFs/JGBAQEp58dw/DUvD1lTo0u5IBFCiveskgBc=";
    fetcherVersion = 2;
  };
  nativeBuildInputs = [
    pnpm.configHook
    nodejs
  ];

  buildPhase = ''
    runHook preBuild
    pnpm run build
  '';

  installPhase = ''
    mkdir -p $out/share/millennium/assets
    cp -r core $out/share/millennium/assets/core
    cp -r pipx $out/share/millennium/assets/pipx
    cp -r .millennium  $out/share/millennium/assets/.millennium
    cp plugin.json $out/share/millennium/assets
    cp requirements.txt $out/share/millennium/assets
  '';
}
