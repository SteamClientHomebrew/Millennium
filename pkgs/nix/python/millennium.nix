{pkgs}: 
pkgs.python311Packages.buildPythonPackage {
  pname = "millennium";
  version = "git";

  src = ../../sdk/python-packages/millennium;
  patches = [
    ./paths.patch
  ];
  postUnpack = ''
    cp ${../../sdk/package.json} ./millennium/package.json
    cp ${../../sdk/README.md} ./millennium/README.md
  '';
}