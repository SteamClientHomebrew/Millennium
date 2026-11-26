# macOS loader

On macOS Millennium loads through the `millennium` command
(`src/bootstrap/macos/cli/`): it stages the runtime and loads it into the
running Steam. It uses no launcher, opens no debug port and changes no Steam
file, so a Steam update needs nothing re-applied.

```sh
# build the runtime (engine, luavm, bootstrap, helpers, and millennium)
cmake --preset osx-release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build/osx-release --target macos_runtime

# stage it, then load it into the running Steam
build/osx-release/millennium setup
build/osx-release/millennium load
```

`openssl@3` is required or curl's `find_package(OpenSSL)` fails at configure.

Commands, signing, and how the load works:
[`cli/README.md`](../../src/bootstrap/macos/cli/README.md).
