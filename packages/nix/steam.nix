{
  steam,
  openssl,
  pkgsi686Linux,
  millennium-python,
  millennium-shims,
  millennium-assets,
  millennium,
  extraPkgs ? (_: [ ]),
  extraLibraries ? (_: [ ]),
  extraEnv ? { },
  extraProfile ? "",
  ...
}@args:
let
  millenniumPkgs = [
    millennium-assets
    millennium-shims
    millennium-python
    pkgsi686Linux.openssl
  ];

  millenniumLibs = [
    millennium
    pkgsi686Linux.openssl
    openssl
  ];

  millenniumEnv = {
    OPENSSL_CONF = "/dev/null";
    STEAM_RUNTIME_LOGGER = "0";
    MILLENNIUM_RUNTIME_PATH = "${millennium}/lib/libmillennium_x86.so";
    LD_LIBRARY_PATH = "${pkgsi686Linux.openssl.out}/lib:$LD_LIBRARY_PATH";
  };

  millenniumProfile = ''
    MILLENNIUM_VENV="$HOME/.local/share/millennium/.venv"
    VENV_PYTHON="$MILLENNIUM_VENV/bin/python3.11"
    EXPECTED_PYTHON=$(readlink -f "${millennium-python}/bin/python3.11")

    if [ -d "$MILLENNIUM_VENV" ]; then
      if [ ! -x "$VENV_PYTHON" ] || [ "$(readlink -f "$VENV_PYTHON")" != "$EXPECTED_PYTHON" ]; then
        echo "[Millennium Steam] venv detected as broken or outdated. Rebuilding"
        rm -rf "$MILLENNIUM_VENV"
      else
        echo "[Millennium Steam] venv is up-to-date"
      fi
    else
      echo "[Millennium Steam] venv not found. Millennium will set it up on next launch"
    fi

    rm -rf ~/.local/share/Steam/ubuntu12_32/libXtst.so.6
    ln -s ${millennium}/lib/libmillennium_bootstrap_86x.so "$HOME/.local/share/Steam/ubuntu12_32/libXtst.so.6"
  '';

  upstreamArgs = removeAttrs args [
    "steam"
    "openssl"
    "pkgsi686Linux"
    "millennium-python"
    "millennium-shims"
    "millennium-assets"
    "millennium"
  ];
in
steam.override (
  upstreamArgs
  // {
    extraPkgs = pkgs: millenniumPkgs ++ (extraPkgs pkgs);
    extraLibraries = pkgs: millenniumLibs ++ (extraLibraries pkgs);
    extraEnv = extraEnv // millenniumEnv;
    extraProfile = millenniumProfile + "\n" + extraProfile;
  }
)
