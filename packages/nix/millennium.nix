{
  lib,
  stdenv,
  callPackage,
  fetchFromGitHub,

  inputs,
  millennium-shims,
  millennium-assets,
  millennium-frontend,
  millennium-32,
  millennium-64,
}:
stdenv.mkDerivation {
  pname = "millennium";
  version = "2.34.0";

  phases = [
    "installPhase"
    "fixupPhase"
  ];

  installPhase = ''
    mkdir -p $out/lib

    echo "[Nix Millennium] Merging 32-bit libraries..."
    cp -v ${millennium-32}/lib/*.so $out/lib/

    echo "[Nix Millennium] Merging 64-bit libraries..."
    cp -v ${millennium-64}/lib/*.so $out/lib/
  '';

  passthru = {
    assets = millennium-assets;
    shims = millennium-shims;
    frontend = millennium-frontend;
  };

  meta = {
    homepage = "https://steambrew.app/";
    license = lib.licenses.mit;
    description = "Modding framework to create, manage and use themes/plugins for Steam";

    longDescription = "An open-source low-code modding framework to create, manage and use themes/plugins for the desktop Steam Client without any low-level internal interaction or overhead";

    maintainers = [
      lib.maintainers.trivaris
    ];

    platforms = [
      "x86_64-linux"
    ];
  };
}
