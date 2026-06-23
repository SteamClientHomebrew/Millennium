#!/usr/bin/env node
"use strict";

const { execFileSync } = require("child_process");
const path = require("path");
const os = require("os");
const fs = require("fs");

const platform = os.platform(); // 'win32', 'linux', 'darwin'
const arch = os.arch(); // 'x64', 'arm64'
const ext = platform === "win32" ? ".exe" : "";

const bin = path.join(
  __dirname,
  "..",
  "binaries",
  `starlight-${platform}-${arch}${ext}`,
);

if (!fs.existsSync(bin)) {
  console.error(`starlight: no binary for ${platform}/${arch} at ${bin}`);
  console.error(
    "Pre-built binaries are distributed via CI. To build from source: cargo build --release",
  );
  process.exit(1);
}

try {
  execFileSync(bin, process.argv.slice(2), { stdio: "inherit" });
} catch (e) {
  process.exit(e.status ?? 1);
}
