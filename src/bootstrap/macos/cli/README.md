# `millennium` (macOS)

```
millennium install [--from DIR]  setup, self-sign, and load on every Steam launch
millennium setup [--from DIR]    stage the built runtime
millennium load                  load Millennium into the running Steam
millennium watch                 load on every Steam launch (run by the autostart agent)
millennium status                show the staged release and Steam state
millennium uninstall             remove the runtime and autostart; keep plugins and config
```

## Signing

`load` needs `com.apple.security.cs.debugger`, which macOS honors on an ad-hoc
signature for same-user debugging. `install` signs `millennium` that way, so no
Apple Developer account or notarization is involved. macOS asks for
authorization on the first load.

## Staging

`setup` copies the build into `runtime/release-<hash>` (a SHA-256 of the
payload) and points the `current` symlink at it. A running Steam keeps the
files it mapped; the next load uses `current`. Restaging the same build changes
nothing, and older releases are removed.

## Loading

1. `load` takes `steam_osx`'s task port and has a new thread `dlopen` the
   bootstrap (`../dylib_bootstrap.cc`). The bootstrap starts the runtime and sets
   `DYLD_INSERT_LIBRARIES` to the child hook, restarting the CEF browser if it is
   already up.
2. In the browser, the child hook (`../child_hook.c`) loads `hhx64` and
   interposes `dlsym`, so Steam's lookup of `cef_browser_host_create_browser`
   returns `hhx64`'s wrapper. Every process it lands in removes it from
   `DYLD_INSERT_LIBRARIES`, so it never spreads past steam_osx's children.
3. `hhx64` drives CEF's in-process DevTools and talks to the runtime over unix
   sockets in `~/Library/Application Support/Millennium/`
   (`src/instrumentation/loopback/macos/`). It stays off if Steam's CEF major
   differs from the one its struct layout was taken from.

`watch` waits for `NSWorkspace` launch notifications (`native.m`), so it loads
about a second after Steam starts, before the browser exists.

A Rosetta-translated task accepts only arm64 thread state, so `load` refuses a
Steam started under Rosetta.

## Logs

```sh
log stream --info --predicate 'subsystem == "app.steambrew.millennium"'
```

## Build

`./build.sh` compiles `millennium` for this Mac and signs it ad hoc.
