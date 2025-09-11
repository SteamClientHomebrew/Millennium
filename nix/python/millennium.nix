{ pkgs, sdk }:
pkgs.python311Packages.buildPythonPackage {
  pname = "millennium";
  version = "git";

  src = sdk + "/python-packages/millennium";
  # Avoid patch file fragility; do in-place substitutions
  postPatch = ''
    substituteInPlace setup.py \
      --replace "../../package.json" "./package.json" \
      --replace "../../README.md" "./README.md"
  '';
  preBuild = ''
    cp ${sdk}/package.json ./package.json
    cp ${sdk}/README.md ./README.md
  '';
}
