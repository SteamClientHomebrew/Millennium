{
  bun,
  cacert,
  nodejs,

  lib,
  stdenv,

  millennium-src,
  ...
}:
let
  bunDeps = stdenv.mkDerivation {
    pname = "millennium-shims-bun-deps";
    version = "2.36.0";

    src = millennium-src;

    nativeBuildInputs = [ bun cacert ];

    buildPhase = ''
      export HOME=$TMPDIR
      bun install --cwd src/typescript --frozen-lockfile
    '';

    installPhase = ''
      cp -rL --no-preserve=mode $HOME/.bun/. $out
    '';

    outputHashMode = "recursive";
    outputHashAlgo = "sha256";
    outputHash = "sha256-ErSfKormY1L+f8rSvJwn1cozsDC+SlWCb7IHGIC82pk=";
  };
in
stdenv.mkDerivation {
  pname = "millennium-shims";
  version = "2.36.0";

  src = millennium-src;

  nativeBuildInputs = [ bun nodejs ];

  buildPhase = ''
    runHook preBuild

    export HOME=$(mktemp -d)
    cp -r ${bunDeps} $HOME/.bun
    chmod -R u+w $HOME/.bun
    bun install --cwd src/typescript --frozen-lockfile
    patchShebangs src/typescript

    # Disable bun auto-install to prevent network access in later build steps
    cat >> src/typescript/bunfig.toml <<'EOF'
[install]
auto = "disable"
EOF

    tsc_js=$(find "$PWD/src/typescript/node_modules/.bun" -name "tsc.js" -path "*/typescript/lib/tsc.js" | head -1)
    rollup_bin=$(find "$PWD/src/typescript/node_modules/.bun" -name "rollup" -path "*/rollup/dist/bin/rollup" | head -1)
    (cd src/typescript/sdk/packages/client && node "$tsc_js" -b)
    (cd src/typescript/sdk/packages/cdp-isolated-ctx && node "$rollup_bin" -c)
    (cd src/typescript/sdk/packages/loader && node "$rollup_bin" -c)

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/millennium/shims
    cp -r src/typescript/sdk/packages/loader/build/* $out/share/millennium/shims/
    cp src/typescript/sdk/packages/cdp-isolated-ctx/build/cdp-isolated-ctx.js $out/share/millennium/shims/

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
}
