# macOS loader

On macOS Millennium loads through one command, `millennium`, built with the
engine (`src/bootstrap/macos/cli/`). It stages the runtime and loads it into the
Steam you already have open.

```sh
# build the runtime (engine, luavm, bootstrap, helpers, and millennium)
cmake --preset osx-release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@3)
cmake --build build/osx-release --target macos_runtime

# stage it, then load it into the running Steam
build/osx-release/millennium setup
millennium load
```

`openssl@3` is required or curl's `find_package(OpenSSL)` fails at configure.

- `setup` copies the build into `~/Library/Application Support/Millennium/runtime`.
- `load` loads the bootstrap into the running `steam_osx`; `watch` does that on
  every Steam launch; `status` shows state; `uninstall` removes the staged
  runtime + autostart (plugins and config are kept).
- `install` is `setup` plus ad-hoc self-signing and a `watch` LaunchAgent — what
  the end-user one-liner (`curl -fsSL https://steambrew.app/install.sh | bash`)
  runs. No Apple Developer account or notarization: the binary is signed on the
  user's own machine, and the first load asks for one macOS authorization.

You open the real `/Applications/Steam.app` yourself and `millennium` loads into
it afterwards, like the Windows and Linux builds load into their Steam — no
wrapper app, no debug port, and no Steam file is changed, so a Steam update needs
nothing re-applied. Internals: [`../../src/bootstrap/macos/cli/`](../../src/bootstrap/macos/cli/).
