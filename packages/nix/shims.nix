{
  nodejs_20,
  pnpm_9,
  fetchPnpmDeps,
  pnpmConfigHook,

  lib,
  stdenv,

  millennium-src,
  ...
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "millennium-shims";
  version = "2.36.0";

  src = millennium-src;

  nativeBuildInputs = [
    nodejs_20
    pnpm_9
    pnpmConfigHook
  ];

  pnpmRoot = "src/typescript/sdk";

  pnpmDeps = fetchPnpmDeps {
    inherit (finalAttrs) version pname;
    pnpm = pnpm_9;
    src = "${finalAttrs.src}/src/typescript/sdk";
    fetcherVersion = 3;
    hash = "sha256-H7k+nkNCb4yuaXcZVmfMI0sqgdYgTO3C2MlXPpYX0x0=";
  };

  buildPhase = ''
    runHook preBuild

    pnpm --dir src/typescript/sdk run build

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/millennium/shims
    cp -r src/sdk/packages/loader/build/* $out/share/millennium/shims/

    runHook postInstall
  '';

  meta = {
    homepage = "https://steambrew.app/";
    license = lib.licenses.mit;
    description = "Millennium Shims to inject into Steam";

    maintainers = [
      lib.maintainers.trivaris
    ];

    platforms = [
      "x86_64-linux"
    ];
  };
})
