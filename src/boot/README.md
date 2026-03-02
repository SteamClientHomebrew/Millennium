> [!WARNING]  
> Note that all sub folders in this directory are "separate" projects from Millennium, they do not compile into the same binary.
> They are shims that are required for bootstrapping Millennium into Steam.
>
> -   Windows -> version.dll || user32.dll (both work, user32.dll is legacy and deprecated as it has some caveats)
> -   Linux -> libXtst.so.6 proxy hook
> -   MacOS -> `steam_osx` wrapper + bootstrap dylib preload + `execve` reinjection into Steam main process + child hook that loads `hhx64` in Steam Helper
> -   MacOS (experimental passive mode) -> `libtier0_s.dylib` reexport proxy installed by `scripts/install_macos.sh`
>     -   Steam verifies and repairs `libtier0_s.dylib` by default.
>     -   For direct `Steam.app` launches without an external launcher, use `scripts/install_macos.sh --steam-cfg-inhibit-all` to write `BootStrapperInhibitAll=enable` into `Steam.cfg`.
>     -   The tier0 proxy creates Steam's `.cef-enable-remote-debugging` marker on each launch so Millennium frontend injection and `steam://millennium/settings` can attach.
>     -   Optional: add `--replace-icon` during install/repair to replace Steam icon with `src/boot/macos/AppIcon.icns` (`--restore-icon` or `--uninstall` restores backup).
>     -   The icon step also clears bundle-level `Icon\r` / `com.apple.FinderInfo` overrides when present, because they can shadow `Steam.icns`.
>     -   This suppresses Steam updates while enabled. Use `--restore-steam-cfg` before updating Steam.
