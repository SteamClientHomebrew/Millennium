{
  nodejs_20,
  pnpm_9,
  fetchPnpmDeps,
  pnpmConfigHook,
  stdenv,
  millennium-src,
  ...
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "millennium-shims";
  version = "5.8.3";

  src = millennium-src;

  nativeBuildInputs = [
    nodejs_20
    pnpm_9
    pnpmConfigHook
  ];

  pnpmRoot = "src/sdk";

  pnpmDeps = fetchPnpmDeps {
    inherit (finalAttrs) version pname;
    pnpm = pnpm_9;
    src = "${finalAttrs.src}/src/sdk";
    fetcherVersion = 3;
    hash = "sha256-NGq5c1E8yM1hwHvVmjtTnReVrXSxb+AK1Qv4K0FsNDg=";
  };

  buildPhase = ''
    runHook preBuild

    pnpm --dir src/sdk run build

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/shims
    cp -r src/sdk/packages/loader/build/* $out/share/shims/

    runHook postInstall
  '';

})