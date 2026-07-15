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

  steamrt3Check = ''
    check_steamrt3_incompatibility() {
        local steam="/home/$(logname)/.local/share/Steam"
        local -a required_markers=(
            "$steam/package/beta"
            "$steam/.steam-enable-steamrt64-client"
            "$steam/steamrt64/steam"
        )

        for marker in "''${required_markers[@]}"; do
            [[ -f "$marker" ]] || return 0
        done

        echo -e "\e[1;33m"
        echo -e "\n  WARNING: SteamRT3 client detected\n"
        echo -e "  Millennium does not support the experimental SteamRT3 client."
        echo -e "  For Millennium to work correctly, please either:"
        echo -e "  - disable the \"Use experimental SteamRT3 Steam Client\" option"
        echo -e "  - opt out of the Steam Beta entirely\n"
        echo -e "\e[0m"
    }

    check_steamrt3_incompatibility
  '';

  millenniumProfile = ''
    ln -sf ${millennium}/lib/libmillennium_bootstrap_x86.so "$HOME/.local/share/Steam/ubuntu12_32/libXtst.so.6"
    ln -sf ${millennium}/lib/libmillennium_bootstrap_hhx64.so "$HOME/.local/share/Steam/ubuntu12_64/libXtst.so.6"

    ${steamrt3Check}
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
