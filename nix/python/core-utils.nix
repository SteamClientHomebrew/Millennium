{pkgs}: 
pkgs.python311Packages.buildPythonPackage {
  pname = "millennium-core-utils";
  version = "git";

  src = ../../sdk/python-packages/core-utils;
  patches = [
    ./paths.patch
  ];
  postUnpack = ''
    cp ${../../sdk/package.json} ./millennium/package.json
    cp ${../../sdk/README.md} ./millennium/README.md
  '';
}