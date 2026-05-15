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
    pname = "millennium-frontend-bun-deps";
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
    outputHash = "sha256-++emopf6oFSA0oLsp1i8SjQRzUtuYnrZ2QsFEaeu1k0=";
  };
in
stdenv.mkDerivation {
  pname = "millennium-frontend";
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

    export PATH="$PWD/src/typescript/node_modules/.bin:$PATH"
    tsc_js=$(find "$PWD/src/typescript/node_modules/.bun" -name "tsc.js" -path "*/typescript/lib/tsc.js" | head -1)
    rollup_bin=$(find "$PWD/src/typescript/node_modules/.bun" -name "rollup" -path "*/rollup/dist/bin/rollup" | head -1)
    (cd src/typescript/sdk/packages/client && node "$tsc_js" -b)
    (cd src/typescript/sdk/packages/browser && node "$tsc_js" -b)
    (cd src/typescript/ttc && node "$rollup_bin" -c)
    (cd src/typescript/frontend && bun run prod)

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/frontend/
    cp -r src/typescript/.frontend.bin $out/share/frontend/frontend.bin

    runHook postInstall
  '';

  meta = {
    homepage = "https://steambrew.app/";
    license = lib.licenses.mit;
    description = "Frontend for Millennium";

    maintainers = [
      lib.maintainers.trivaris
    ];

    platforms = [
      "x86_64-linux"
    ];
  };
}
