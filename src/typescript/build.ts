/**
 * ==================================================
 *   _____ _ _ _             _
 *  |     |_| | |___ ___ ___|_|_ _ _____
 *  | | | | | | | -_|   |   | | | |     |
 *  |_|_|_|_|_|_|___|_|_|_|_|_|___|_|_|_|
 *
 * ==================================================
 *
 * Copyright (c) 2026 Project Millennium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// import reference bun types. works without type decls.
/// <reference types="bun-types" />

import { $ } from "bun";
import chalk from "chalk";
import { createHash } from "crypto";
import { existsSync, readFileSync, readdirSync, statSync } from "fs";
import path from "path";

const ROOT = import.meta.dir;
const CACHE_FILE = path.join(ROOT, ".build-cache.json");

// Directories/files that are never part of the source hash.
const SKIP_DIRS = new Set([
  "node_modules",
  "build",
  "dist",
  ".turbo",
  ".cache",
]);

const startTime = performance.now();

// Paths to hash for each package (relative to ROOT).
// Directories are walked recursively; SKIP_DIRS entries are ignored.
const packageSources: Record<string, string[]> = {
  "@steambrew/ttc": ["ttc/src", "ttc/rollup.config.js", "ttc/package.json"],
  "@steambrew/client": [
    "sdk/packages/client/src",
    "sdk/packages/client/tsconfig.json",
    "sdk/packages/client/package.json",
  ],
  "@steambrew/webkit": [
    "sdk/packages/browser/src",
    "sdk/packages/browser/tsconfig.json",
    "sdk/packages/browser/package.json",
  ],
  "@steambrew/api": [
    "sdk/packages/loader/src",
    "sdk/packages/loader/rollup.config.js",
    "sdk/packages/loader/package.json",
  ],
  core: ["frontend", "../locales"], // entire dir; node_modules/build are skipped
};

// Package name → workspace directory (relative to ROOT).
// Used with --cwd so package-local node_modules/.bin is in PATH.
const packageDirs: Record<string, string> = {
  "@steambrew/ttc": "ttc",
  "@steambrew/client": "sdk/packages/client",
  "@steambrew/webkit": "sdk/packages/browser",
  "@steambrew/api": "sdk/packages/loader",
  core: "frontend",
};

// When a package is rebuilt its dependents must also rebuild even if their
// own sources are unchanged (because the dep's build output changed).
const dependsOn: Record<string, string[]> = {
  "@steambrew/api": ["@steambrew/client"],
  core: ["@steambrew/client", "@steambrew/webkit"],
};

function hashPath(absPath: string, h: ReturnType<typeof createHash>): void {
  if (!existsSync(absPath)) return;
  const s = statSync(absPath);
  if (s.isFile()) {
    h.update(absPath + ":");
    h.update(readFileSync(absPath));
  } else if (s.isDirectory()) {
    for (const entry of readdirSync(absPath).sort()) {
      if (SKIP_DIRS.has(entry)) continue;
      hashPath(path.join(absPath, entry), h);
    }
  }
}

function computeHash(filter: string): string {
  const h = createHash("sha256");
  for (const rel of packageSources[filter] ?? [])
    hashPath(path.join(ROOT, rel), h);
  return h.digest("hex");
}

// Load persisted hashes from last build.
let cache: Record<string, string> = {};
if (existsSync(CACHE_FILE)) {
  try {
    cache = JSON.parse(readFileSync(CACHE_FILE, "utf-8"));
  } catch {
    /* corrupt cache — rebuild everything */
  }
}

const steps = [
  "@steambrew/ttc",
  "@steambrew/client",
  "@steambrew/webkit",
  "@steambrew/api",
  "core",
];

const rebuilt = new Set<string>();

for (const filter of steps) {
  const hash = computeHash(filter);
  const depDirty = (dependsOn[filter] ?? []).some((d) => rebuilt.has(d));

  if (cache[filter] === hash && !depDirty) {
    console.log(`${chalk.dim("skip")}     ${chalk.yellow(filter)}`);
    continue;
  }

  process.stdout.write(`building ${chalk.yellow(filter)}... `);
  const start = performance.now();
  try {
    await $`bun run --cwd ${packageDirs[filter]} build`.quiet();
    rebuilt.add(filter);
    cache[filter] = hash;
    console.log(
      chalk.dim(`done (${((performance.now() - start) / 1000).toFixed(2)}s)`),
    );
  } catch (e: any) {
    console.log("failed\n");
    if (e.stdout?.length) process.stderr.write(e.stdout.toString());
    if (e.stderr?.length) process.stderr.write(e.stderr.toString());
    if (!e.stdout?.length && !e.stderr?.length) process.stderr.write(String(e));
    process.exit(1);
  }
}

// Persist hashes so the next run can skip unchanged packages.
await Bun.write(CACHE_FILE, JSON.stringify(cache, null, 2));

console.log(
  `\n${chalk.green("Finished")} build in ${((performance.now() - startTime) / 1000).toFixed(2)}s`,
);
