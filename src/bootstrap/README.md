> [!WARNING]  
> Note that all sub folders in this directory are "separate" projects from Millennium, they do not compile into the same binary.
> They are shims that are required for bootstrapping Millennium into Steam.
>
- Windows -> wsock32.dll
- Linux (32-bit client, `ubuntu12_32/steam`) -> `libXtst.so.6` proxy hook (`libmillennium_bootstrap_x86.so`, built by the opt-in `-x86` presets)
- Linux (SteamRT3 64-bit client, `steamrt64/steam`) -> `libXtst.so.6` proxy hook (`libmillennium_bootstrap_x64.so`, built by the default Linux presets)
- macOS (Install, default) -> `Steam Millennium.app` wrapper + runtime payload installed by `scripts/install_macos.sh`
> -   macOS (Legacy install) -> `libtier0_s.dylib` reexport proxy via `scripts/install_macos.sh --tier0-legacy`
> -   macOS (Debug) -> `steam_osx` wrapper + bootstrap dylib preload + `execve` reinjection into Steam main process + child hook that loads `hhx64` in Steam Helper

## Linux runtime dependencies

Both the x86 client and SteamRT3 are supported. The 64-bit SteamRT3 tree is the default Linux build; the 32-bit client tree is opt-in through the `-x86` presets. Build both when packaging for both clients; the x64 tree can also build independently.

Non-Nix 64-bit payloads and helpers use `DT_RPATH` to find host libraries exposed inside SteamRT3 under `/run/host`. This includes the 64-bit helpers produced by the legacy build. The default directories are `/run/host/usr/lib64`, `/run/host/usr/lib`, and `/run/host/usr/lib/x86_64-linux-gnu`. Override the CMake list `MILLENNIUM_LINUX_HOST_LIBRARY_PATHS` for other layouts, or set it to an empty string to disable these paths. Both build and install RPATHs are set because package recipes also copy artifacts directly from the build tree. Nix builds retain their store-path configuration.

The proxies load Millennium with `dlopen`, so the libraries they load must not be linked with `-Wl,-z,nodlopen`.
