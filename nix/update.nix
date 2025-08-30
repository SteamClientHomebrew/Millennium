{
  lib,
  writeShellScriptBin,
  nix-update,
}:
writeShellScriptBin "update-pnpm-hashes" ''
  ${lib.getExe nix-update} --version skip --flake assets
  ${lib.getExe nix-update} --version skip --flake shims
''
