{
  steam,
  stdenv,
  millennium-python,
  millennium-shims,
  millennium-assets,
  millennium,
  ...
}:
steam.override {
  extraPkgs = pkgs: [
    millennium-assets
    millennium-shims
    millennium-python
  ];

  extraLibraries = pkgs: [
    millennium
  ];

  extraEnv = {
    OPENSSL_CONF = "/dev/null";
    STEAM_RUNTIME_LOGGER = "0";
    MILLENNIUM_RUNTIME_PATH = "${millennium}/lib/libmillennium_x86.so";
    MILLENNIUM_HELPER_PATH = "${millennium}/lib/libmillennium_hhx64.so";
  };

  extraProfile = ''
    rm -rf ~/.local/share/Steam/ubuntu12_32/libXtst.so.6
    ln -s ${millennium}/lib/libmillennium_bootstrap_86x.so "$HOME/.local/share/Steam/ubuntu12_32/libXtst.so.6"
  '';

}
