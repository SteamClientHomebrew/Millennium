# `millennium` (macOS)

`millennium` stages the runtime, loads it into the running Steam, and manages
autostart.

```
millennium install [--from DIR] setup + self-sign + autostart on every launch
millennium setup [--from DIR]   stage the built runtime (release-<hash> + current)
millennium load                 load Millennium into the running Steam
millennium watch                load on every Steam launch (sleeps until Steam starts)
millennium status               show the staged release and Steam state
millennium uninstall            remove the staged runtime + autostart
```

## Signing (no Apple Developer account)

`load` needs `com.apple.security.cs.debugger`, which macOS honors on an ad-hoc
signature for same-user debugging. `install` self-signs `millennium` locally with
that entitlement (`millennium.entitlements`); this needs no paid account and no
notarization.
The first load prompts once for authorization, then remembers it.

## Staging

`setup` lays the payload down as `runtime/release-<hash>` (hash = SHA-256 of the
payload) and swaps the relative `current` symlink onto it by rename. A running
Steam keeps the files it already mapped; the next launch picks up `current`.
Re-staging an identical build is a no-op, and old releases are pruned.

## How `load` works

`load` takes the running `steam_osx`'s task port (same user, no sudo) and has it
`dlopen` the bootstrap on a short-lived pthread. The bootstrap starts the runtime
and sets `DYLD_INSERT_LIBRARIES` to the child hook (`../dylib_bootstrap.cc`). If
the CEF browser is already up, it restarts it once so the new one inherits that.

In the browser, the child hook (`../child_hook.c`) loads `hhx64` and interposes
`dlsym`: Steam Helper resolves `cef_browser_host_create_browser` by name, and
gets `hhx64`'s wrapper instead. CEF's code is never patched. `hhx64` drives CEF's
in-process DevTools and talks to the runtime over unix sockets in
`~/Library/Application Support/Millennium/` (`../cef_cdp_bridge.c`). The bridge
stays off if the CEF major differs from the layout it was built for.

## How `watch` works

The autostart agent runs `watch`, which asks `NSWorkspace` for launch
notifications and sleeps until Steam starts (`watch.m`). It loads within about a
second of the launch, usually before Steam opens its browser, so that browser
needs no restart.

## Logs

Every part logs to the unified log under the `app.steambrew.millennium`
subsystem (categories `agent`, `bootstrap`, `child-hook`, `cdp`):

```sh
log stream --info --predicate 'subsystem == "app.steambrew.millennium"'
```

## Build

`./build.sh` compiles `millennium` and signs it ad hoc, the same way `install`
signs it on the user's machine.
