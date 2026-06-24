#!/usr/bin/env node
"use strict";

const { execFileSync } = require("child_process");
const path = require("path");
const os = require("os");
const fs = require("fs");

const platform = os.platform(); // 'win32', 'linux', 'darwin'
const arch = os.arch(); // 'x64', 'arm64'
const ext = platform === "win32" ? ".exe" : "";

const packagedBin = path.join(
  __dirname,
  "..",
  "binaries",
  `starlight-${platform}-${arch}${ext}`,
);

const localBin = [
  path.join(__dirname, "..", "..", "target", "debug", `starlight${ext}`),
].find(fs.existsSync);

const bin = fs.existsSync(packagedBin) ? packagedBin : localBin;

if (!bin) {
  console.error(`starlight: no binary for ${platform}/${arch}`);
  process.exit(1);
}

try {
  execFileSync(bin, process.argv.slice(2), {
    stdio: "inherit",
    env: { ...process.env, STARLIGHT_TYPES_DIR: path.join(__dirname, "millennium", "types") },
  });
} catch (e) {
  process.exit(e.status ?? 1);
}
