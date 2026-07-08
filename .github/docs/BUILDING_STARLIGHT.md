# Building from Source

## Linux

### Prerequisites

**Arch Linux** (other distros need equivalent packages):

[`rust`](https://repology.org/project/rust/versions)

### Build

```bash
# binary at target/release/starlight
$ cargo build --release

# build debug
$ cargo build 
```

## Windows

### Prerequisites

- [Rust](https://rust-lang.org/)

### Build

```bash
# binary at target/release/starlight
$ cargo build --release

# build debug
$ cargo build 
```

# Using starlight

To compile plugins using your locally compiled starlight:

```bash
$ cd ./plugin

# use locally compiled starlight to compile plugin
$ cargo run --manifest-path /path/to/starlight/Cargo.toml
```
