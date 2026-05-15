{
  steam,
  openssl,
  pkgsi686Linux,
  lib,
  millennium,
  extraPkgs ? (_: [ ]),
  extraLibraries ? (_: [ ]),
  extraEnv ? { },
  extraProfile ? "",
  ...
}@args:
let
  millenniumLibs = [
    millennium
    pkgsi686Linux.openssl
    openssl
  ];

  millenniumEnv = {
    # consumed by millennium internally.
    MILLENNIUM_RUNTIME_PATH = "${millennium}/lib/libmillennium_x86.so";
  };

  millenniumProfile = ''
    ln -sf ${millennium}/lib/libmillennium_bootstrap_x86.so "$HOME/.local/share/Steam/ubuntu12_32/libXtst.so.6"
    ln -sf ${millennium}/lib/libmillennium_bootstrap_hhx64.so "$HOME/.local/share/Steam/ubuntu12_64/libXtst.so.6"
  '';

  upstreamArgs = removeAttrs args [
    "steam"
    "openssl"
    "pkgsi686Linux"
    "millennium"
  ];
in
steam.override (
  upstreamArgs
  // {
    extraPkgs      = pkgs: extraPkgs pkgs;
    extraLibraries = pkgs: millenniumLibs ++ (extraLibraries pkgs);
    extraEnv       = extraEnv // millenniumEnv;
    extraProfile   = millenniumProfile + "\n" + extraProfile;
  }
)
