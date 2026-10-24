> [!WARNING]  
> Note that all sub folders in this directory are "separate" projects from Millennium, they do not compile into the same binary.
> They are shims that are required for bootstrapping Millennium into Steam.
>
- Windows -> wsock32.dll
- Linux (32-bit client, `ubuntu12_32/steam`) -> `libXtst.so.6` proxy hook (`libmillennium_bootstrap_x86.so`)
- Linux (SteamRT3 64-bit client, `steamrt64/steam`) -> `libXtst.so.6` proxy hook (`libmillennium_bootstrap_x64.so`, built with `-DMILLENNIUM_LINUX_X64=ON`)
- macOS (Install, default) -> `Steam Millennium.app` wrapper + runtime payload installed by `scripts/install_macos.sh`
> -   macOS (Legacy install) -> `libtier0_s.dylib` reexport proxy via `scripts/install_macos.sh --tier0-legacy`
> -   macOS (Debug) -> `steam_osx` wrapper + bootstrap dylib preload + `execve` reinjection into Steam main process + child hook that loads `hhx64` in Steam Helper
