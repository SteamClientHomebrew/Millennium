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
`lib32-gcc-libs`
`lib32-openssl`
`lib32-libidn2`
`lib32-xz`
`lib32-zstd`
`lib32-brotli`
`lib32-libnghttp2`
`lib32-libpsl`

### Build

```bash
$ cmake --preset linux-debug
# We also ship `linux-release`, run
# cmake --list-presets to list all presets

# You can also provide -j$(nproc) to build multithreaded.
$ cmake --build build
```

### Dev Environment Setup

After building, run the setup script to symlink the build outputs into Steam's runtime directories.
This only needs to be run **once** — the symlinks point into the build tree, so rebuilding automatically picks up changes without re-running the script. Just restart Steam after rebuilding.

```bash
# insert Millennium into your Steam installation
$ ./scripts/linux/setup_devenv.sh

# remove Millennium and restore Steam to its stock state
$ ./scripts/linux/destroy_devenv.sh
```

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
