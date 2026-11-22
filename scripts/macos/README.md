# Updating the macOS runtime

Start with a configured Release build and compiled frontend assets.

```sh
cmake --build build/osx-release --target macos_runtime
./scripts/macos/install_macos.sh --update-runtime --build-dir build/osx-release
```

`macos_runtime` builds the core library, its helpers, the Lua host, the bootstrap and the launcher. The installer copies them into `~/Library/Application Support/Millennium/runtime/release-<hash>` and points `current` at that directory. `Steam Millennium.app` links through `current`, so the next launch uses the new release while a running Steam keeps the old one. Installing the same build again changes nothing. `--status` prints the installed version.
