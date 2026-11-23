# `millennium` (macOS)

The macOS command for Millennium. It stages the runtime, loads it into the Steam
you already have open, and reports state — one binary, no launcher, no debug
port, no changes to any Steam file.

```
millennium install [--from DIR] setup + self-sign + load on every Steam launch
millennium setup [--from DIR]   copy the built runtime into place
millennium load                 load Millennium into the running Steam
millennium watch                load on every Steam launch (stays resident)
millennium status               show runtime + Steam state
millennium uninstall            remove the staged runtime + autostart
```

## Installing (no Apple Developer account)

The one-line installer (`curl -fsSL https://steambrew.app/install.sh | bash`)
downloads the build and runs `millennium install`. `install` stages the runtime,
clears the download quarantine, **ad-hoc signs** `millennium` locally with the
`cs.debugger` entitlement (macOS honors that for same-user debugging — no paid
account, no notarization), and registers a per-user LaunchAgent that runs
`millennium watch`. From then on you open the real Steam.app and Millennium loads
into it.

The first load asks for one macOS authorization (an admin password, the standard
"take control of another process" prompt); after that it is remembered. No Xcode
and no developer-mode setup are required.

## How `load` works

You open the real `/Applications/Steam.app` and it starts normally. `millennium
load` then loads the bootstrap into the running `steam_osx` the way a debugger
loads a library into a running process: it takes the process's task port (needs
`com.apple.security.cs.debugger`, same user, no sudo) and has the process call
its own `dlopen` on a short-lived pthread. Steam keeps running.

The loaded bootstrap brings up Millennium's CDP channel over Steam's own
`--remote-debugging-pipe` (`../dylib_bootstrap.cc`, `../got_rebind.h`).

## Build

```bash
./build.sh
```

Compiles and ad-hoc signs `millennium` with `cs.debugger` — the same signature
that ships, which `millennium install` re-applies on the user's machine.
