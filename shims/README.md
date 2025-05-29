## Introduction

This sub-project is responsible for transpiling and assembling @steambrew/client and @steambrew/webkit into a bundle that is injectable into the Steam Client.
`StartPreloader` is the main function that is called from the backend of Millennium in C++ with provided parameters.

It's also responsible for connecting to Millennium's IPC to make FFI (foreign function interface) calls between loaded plugin frontends and their respective backend.

## Developing

This bundle is built into `./build`. This folder should be symlinked to `STEAM_PATH/ext/data/shims` on Windows (check root README for location on Unix).
From there, Millennium will automatically handle the rest, it will be injected right into the Steam Client

In the case you've edited @steambrew/client or @steambrew/webkit and are looking to test them,
make sure you've ran `pnpm link --global` from the @steambrew package, and then running `pnpm link @steambrew/the_package_name --global` from here.
This will make sure `pnpm run build` uses your local dev version of your @steambrew package instead of the remote version on the NPM registry.
