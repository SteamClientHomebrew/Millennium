{
  stdenv,
  millennium-src,
  ...
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "millennium-assets";
  version = "1.0.0";

  src = millennium-src;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/share/assets/
    cp -r src/pipx $out/share/assets/pipx

    runHook postInstall
  '';

})
