# macOS loader

On macOS, the `millennium` command stages the runtime and loads it into the
running Steam. It uses no launcher, opens no debug port and changes no Steam
file, so a Steam update needs nothing re-applied.

## Build

Each architecture has its own build: pick `arm64` for Apple silicon or `x86_64`
for Intel. Homebrew's `openssl@3` must be installed for that architecture.

```sh
cmake --preset osx-arm64-release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build/osx-arm64-release --target macos_runtime
```

## Use

```sh
build/osx-arm64-release/millennium setup   # stage the build
build/osx-arm64-release/millennium load    # load into the running Steam
```

Steam must run natively; a Steam started under Rosetta cannot be loaded into.
Commands and internals: [`cli/README.md`](../../src/bootstrap/macos/cli/README.md).
