# Special thanks to @Sk7Str1p3, @mourogurt, @kaeeraa, @mctrxw for help with this flake and packages
{
  description = ''
    Millennium - an open-source low-code modding framework to create, 
    manage and use themes/plugins for the desktop Steam Client
  '';

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    self.submodules = true; # Requires *Nix* >= 2.27
  };

  outputs =
    { nixpkgs, self, ... }:
    let
      pkgs = import nixpkgs {
        system = "x86_64-linux";
        config.allowUnfree = true;
      };
    in
    {
      # TODO: move shell to separate file to
      #       let users without flakes use it too
      devShells."x86_64-linux".default = pkgs.mkShellNoCC {
        stdenv = pkgs.stdenv_32bit;
        name = "Millennium";
        packages = with pkgs; [
          nixd
          nixfmt-rfc-style
          mypy
        ];
      };

      packages."x86_64-linux" = {
        millennium = pkgs.callPackage ./nix/millennium.nix { };
        shims = pkgs.callPackage ./nix/shims.nix { };
        assets = pkgs.callPackage ./nix/assets.nix { };
      };
    };
}
