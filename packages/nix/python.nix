{
  lib,
  pkgsi686Linux,
  fetchurl,
  ...
}:
pkgsi686Linux.stdenv.mkDerivation(finalAttrs: {
  pname = "millennium-python";
  version = "3.11.8";

  src = fetchurl {
    url = "https://www.python.org/ftp/python/3.11.8/Python-3.11.8.tgz";
    sha256 = "sha256-0wGaYTueh2HSYNnr471N9jl23jBGTlwBiVZuGuP2GIk=";
  };

  buildInputs = [
    pkgsi686Linux.zlib
    pkgsi686Linux.ncurses
    pkgsi686Linux.gdbm
    pkgsi686Linux.nss
    pkgsi686Linux.openssl
    pkgsi686Linux.readline
    pkgsi686Linux.libffi
    pkgsi686Linux.bzip2
    pkgsi686Linux.sqlite
    pkgsi686Linux.xz
    pkgsi686Linux.expat
    pkgsi686Linux.util-linux
  ];

  configureFlags = [
    "--enable-shared"
    "--enable-optimizations"
    "--without-ensurepip"
  ];

  preBuild = ''
    export LD_LIBRARY_PATH=$(pwd):$LD_LIBRARY_PATH
  '';

  installPhase = ''
    runHook preInstall

    make altinstall prefix=$out

    rm -rf $out/lib/python3.11/test/
    rm -rf $out/lib/python3.11/__pycache__/
    rm -rf $out/lib/python3.11/config-3.11-*
    rm -rf $out/lib/python3.11/tkinter/
    rm -rf $out/lib/python3.11/idlelib/
    rm -rf $out/lib/python3.11/turtledemo/
    rm -f  $out/lib/libpython3.11.a

    strip $out/bin/python3.11
    
    cp $out/lib/libpython3.11.so $out/lib/libpython-3.11.8.so
    strip $out/lib/libpython-3.11.8.so

    rm -f $out/bin/python3.11-config
    rm -f $out/bin/idle3.11
    rm -f $out/bin/pydoc3.11
    rm -f $out/bin/2to3-3.11

    runHook postInstall
  '';

  postFixup = ''
    patchelf --set-rpath "$out/lib" $out/lib/libpython-3.11.8.so
  '';
})