# Millennium Installer for Bazzite (64-Bit Steam Client)

Community-maintained installer and updater for [Millennium](https://github.com/SteamClientHomebrew/Millennium) (`v3.6.3+`) on **Bazzite** and other Fedora Atomic / OSTree-based distributions.

## Why This Exists
Bazzite uses an immutable root filesystem (`/usr` is read-only) and runs the 64-bit Linux Steam client inside the SteamRT3 container. Standard Linux installers fail when attempting to write directly to `/usr/lib/millennium`, and symlinking across container boundaries breaks bootstrap checks.

This script solves both issues without unlocking or modifying your base OSTree image:
* **`systemd-sysext` Overlay:** Installs the official 64-bit Millennium release binaries into `/var/lib/extensions/millennium/usr/lib/millennium/` and layers them cleanly over `/usr/lib/millennium/` in memory.
* **Automatic OS Update Sync:** Installs a lightweight `systemd` boot service (`millennium-sysext-sync.service`) that keeps the extension's `ID` and `VERSION_ID` synced with `/etc/os-release` after every `ujust update` reboot.
* **Atomic SteamRT64 Hook:** Copies `libmillennium_bootstrap_x64.so` and `libmillennium_hhx64.so` into `~/.local/share/Steam/steamrt64/` and clears stale CEF `htmlcache` so Millennium initializes on the first launch.

## Prerequisites
* **Bazzite** (Desktop or Deck edition) with the standard 64-bit Steam client installed (`v3.6.3+` compatible).
* Standard utilities pre-installed on Bazzite: `curl`, `jq`, `tar`, `sha256sum`, `systemd-sysext`.

## Installation & Updating
Run the same script to perform a fresh install or update to the latest official Millennium release:

```bash
curl -fsSL https://raw.githubusercontent.com/JankyMouse/millennium-bazzite/main/install_millennium_bazzite_64bit.sh -o install_millennium_bazzite.sh
chmod +x install_millennium_bazzite.sh
./install_millennium_bazzite.sh
```

1. Enter your `sudo` password when prompted (needed to configure `/var/lib/extensions/millennium` and the `systemd` sync service).
2. Once the script finishes, launch **Steam**.
3. Open **Steam → Settings → Millennium** to manage your themes and plugins.

*(Note: Clearing Steam's CEF `htmlcache` during installation may cause Steam to open in a smaller default window on the first boot—simply maximize or resize the window once and Steam will remember your layout.)*

## Uninstalling
To completely remove Millennium and revert Steam to stock:

```bash
killall -9 steam 2>/dev/null || true
sudo systemctl disable --now millennium-sysext-sync.service
sudo rm -f /etc/systemd/system/millennium-sysext-sync.service
sudo systemctl daemon-reload
sudo systemd-sysext unmerge
sudo rm -rf /var/lib/extensions/millennium
rm -f ~/.local/share/Steam/steamrt64/libXtst.so.6 ~/.local/share/Steam/steamrt64/libmillennium_hhx64.so
```
