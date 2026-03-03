> [!WARNING]  
> Note that all sub folders in this directory are "separate" projects from Millennium, they do not compile into the same binary.
> They are shims that are required for bootstrapping Millennium into Steam.
>
> -   Windows -> version.dll || user32.dll (both work, user32.dll is legacy and deprecated as it has some caveats)
> -   Linux -> libXtst.so.6 proxy hook
> -   MacOS -> `steam_osx` wrapper + bootstrap dylib preload + `execve` reinjection into Steam main process + child hook that loads `hhx64` in Steam Helper
