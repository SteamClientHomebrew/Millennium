# starlight

A compiler tool for [Millennium](https://github.com/SteamClientHomebrew/Millennium) plugins. 

## Installation

### Via npm (recommended)

```bash
$ npm install -g @steambrew/starlight
```

Pre-built binaries for Linux and Windows are bundled. Node 16+ required.

### From source

```bash
# binary at target/release/starlight
$ cargo build --release
```

## Configuration

starlight is driven by a `millennium.toml` file at the root of your plugin project.

```toml
[plugin]
id          = "my-plugin"
name        = "My Plugin"
version     = "1.0.0"
author      = "you"
description = "Optional description"

# Lua backend (optional)
[backend]
entry   = "backend/main.lua"
sources = ["backend/**/*.lua"]

# Frontend injected into the Steam client UI (optional)
[frontend]
entry = "frontend/index.tsx"

[frontend.globals]
SOME_CONST = "\"hello\""

# WebKit-side injection (optional)
[webkit]
entry = "webkit/index.tsx"

# Dev/watch mode settings (optional)
[dev]
auto_restart       = true          # restart plugin on each rebuild (default: true)
plugin_name        = "my-plugin"   # override if different from plugin.id
socket             = "/tmp/millennium-mep.sock"  # MEP socket path
reload_steamui_when = "never"      # "never" | "backend" | "frontend" | "both"
```

### `[inspect]` section (optional)

Controls how the Lua `inspect()` polyfill renders values in watch mode logs.

```toml
[inspect]
depth       = 2     # table nesting depth (default: 2)
colors      = true  # ANSI color output (default: true)
show_hidden = false # show hidden fields (default: false)
```

## Usage

### Build (pack)

```bash
# Build plugin.star from millennium.toml in the current directory
$ starlight pack

# Specify a different config file
$ starlight --config path/to/millennium.toml pack

# Specify config directory (looks for millennium.toml inside it)
$ starlight --dir path/to/plugin pack

# Write output to a specific path
$ starlight --of dist/my-plugin.star pack

# Debug build (emits source maps and developer tooling)
$ starlight --debug pack

# Release build
$ starlight --release pack
```

`starlight` with no subcommand defaults to `pack`.

### Watch mode

Watches the plugin directory for changes and rebuilds automatically. If a `[dev]` section is configured, it also restarts the plugin via MEP (Millennium Execution Protocol) after each successful build.

```bash
$ starlight watch
$ starlight --debug watch
```

Press `Ctrl+C` to stop.

### Verify

Checks CRC32 integrity of all sections and, if the package is signed, verifies the Ed25519 signature. Requires `STARLIGHT_SIGNING_KEY` to be set when verifying a signed package.

```bash
$ starlight verify plugin.star
```

### Inspect

Prints a summary of all sections and their contents without strict signature checking, useful for debugging packages you don't have the signing key for.

```bash
$ starlight inspect plugin.star
```

### Query

Extracts specific data from a `.star` file for scripted use (e.g. in CI pipelines).

```bash
# Print starlight version that built the file
$ starlight --if plugin.star -q version

# Print metadata section as JSON
$ starlight --if plugin.star -q metadata

# List files in the frontend section
$ starlight --if plugin.star -q frontend

# Extract a specific file from the backend section to stdout
$ starlight --if plugin.star -q backend:main.lua
```

Valid query values: `version`, `metadata`, `frontend`, `frontend:<file>`, `backend`, `backend:<file>`, `webkit`, `webkit:<file>`.

## Code signing

Signing is optional. If you choose to sign your plugin, only you can recognize said signature. 
Millennium will not treat your plugin as signed unless it was via our official PluginDatabase CI.
When `STARLIGHT_SIGNING_KEY` is not set, packages are built unsigned.

### Generate a key pair

```bash
# Using openssl
$ openssl genpkey -algorithm ed25519 -outform DER | tail -c 32 | xxd -p -c 64
```

This prints the 64-character hex private key seed. Store it securely (e.g. as a CI secret).

### Sign during pack

```bash
$ export STARLIGHT_SIGNING_KEY=<64-char-hex>
$ starlight pack
```

The public key is derived from the private key and must be embedded in the Millennium runtime to verify signatures at load time.

## `.star` file format

A `.star` file has the following layout:

```
[shim_len: u32][lua shim: shim_len bytes]
[STAR header: 12 bytes] (magic "STAR", version, section count, flags, shim FNV-1a hash)
[section table: 14 bytes * section_count]
[section data ...]
[Ed25519 signature: 64 bytes] (only if signed)
```

Each section is independently LZ4-compressed, XOR-obfuscated, and parity-woven. Section types:

| ID   | Name     | Contents                        |
|------|----------|---------------------------------|
| 0x01 | metadata | MessagePack-encoded plugin info |
| 0x02 | backend  | Lua source files                |
| 0x03 | frontend | Bundled JS for the Steam UI     |
| 0x04 | webkit   | Bundled JS for the WebKit layer |

## License

See [LICENSE](LICENSE.md).
