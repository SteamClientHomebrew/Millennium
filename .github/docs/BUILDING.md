# Building from Source

## Linux

### Prerequisites

**Arch Linux** (other distros need equivalent packages):

[`rust`](https://repology.org/project/rust/versions)
[`cmake`](https://repology.org/project/cmake/versions)
[`ninja`](https://repology.org/project/ninja/versions)
[`bun`](https://repology.org/project/bun/versions)
[`libx11`](https://repology.org/project/libx11/versions)
[`libxtst`](https://repology.org/project/libxtst/versions)

The 32-bit client tree additionally needs the multilib packages:
`lib32-gcc-libs`
`lib32-openssl`
`lib32-libidn2`
`lib32-xz`
`lib32-zstd`
`lib32-brotli`
`lib32-libnghttp2`
`lib32-libpsl`

### Build

The Linux presets target the 64-bit SteamRT3 Steam client by default:

```bash
$ cmake --preset linux-debug
# We also ship `linux-release`, run
# cmake --list-presets to list all presets

# You can also provide -j$(nproc) to build multithreaded.
$ cmake --build build
```

`linux-release`, `linux-debug` and `linux-arch-pkgbuild` build the 64-bit tree in `build/`. The `-x64` presets (`linux-release-x64`, `linux-debug-x64`, `linux-arch-pkgbuild-x64`) remain as aliases that build the same tree in `build64/` and `build64-debug/`.

For the legacy 32-bit client, use the `-x86` presets, which configure a separate directory with `-m32`:

```bash
$ cmake --preset linux-release-x86
$ cmake --build build32
```

Packaging both clients uses `linux-arch-pkgbuild` (64-bit, `build/`) and `linux-arch-pkgbuild-x86` (32-bit, `build32/`). `linux-debug-x86` builds into `build32-debug/`.

> A build directory configured before the 64-bit default keeps its cached 32-bit flags; configuring a 64-bit build on top of it stops with a pointer-size error. Delete the old build directory and reconfigure after updating.

### Dev Environment Setup

After building, run the setup script to symlink the build outputs into Steam's runtime directories.
This only needs to be run **once** — the symlinks point into the build tree, so rebuilding automatically picks up changes without re-running the script. Just restart Steam after rebuilding.
The script follows the architecture of the given build directory: 64-bit builds link `steamrt64/libXtst.so.6`, 32-bit builds link `ubuntu12_32/` and `ubuntu12_64/`.

```bash
# insert Millennium into your Steam installation
$ ./scripts/linux/setup_devenv.sh

# remove Millennium and restore Steam to its stock state
$ ./scripts/linux/destroy_devenv.sh
```

Use the debug presets for development — they embed the build-tree paths, so the proxies load the freshly built libraries without installing them. `./scripts/linux/setup_devenv.sh build32` sets up a 32-bit build tree.

## Windows

### Prerequisites

- [Visual Studio Build Tools](https://aka.ms/vs/17/release/vs_buildtools.exe) (with the "Desktop development with C++" workload — includes CMake and Ninja, add them to PATH)
- [Bun](https://bun.sh/)
- [Rust](https://rust-lang.org/)

### Build

Open a **Developer PowerShell for VS**, then:

```bash
cmake --preset windows-debug -DMILLENNIUM_BUILD_TO_STEAM_PATH # build into Steam directory  
# We also ship `windows-release`, run
# cmake --list-presets to list all presets

# You can also provide -j<SOME_VALUE> to build multithreaded.
cmake --build build
```

On Windows, CMake automatically detects your Steam installation from the registry and outputs binaries directly into Steam's directory. No additional setup is needed — just restart Steam after building.
