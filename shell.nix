{pkgs ? import <nixpkgs>}:
with pkgs;

mkShell {
  name = "Millennium";
  stdenv = stdenv_32bit;
  packages = [
    nixd
    nixfmt-rfc-style
    mypy
  ];
}