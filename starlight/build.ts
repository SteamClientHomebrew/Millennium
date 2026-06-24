#!/usr/bin/env bun
// @ts-nocheck
import { copyFileSync, chmodSync, mkdirSync, existsSync, readFileSync, writeFileSync, rmSync } from "fs";
import { join } from "path";

const root = import.meta.dir;
const repoRoot = join(root, "..");

function run(cmd: string[], opts: { cwd?: string } = {}) {
	const result = Bun.spawnSync(cmd, { cwd: opts.cwd ?? root, stdio: ["inherit", "inherit", "inherit"] });
	if (result.exitCode !== 0) {
		console.error(`[build] ${cmd.join(" ")} exited ${result.exitCode}`);
		process.exit(result.exitCode ?? 1);
	}
}

function zipDir(srcDir: string, destZip: string) {
	run(["zip", "-qr", destZip, "."], { cwd: srcDir });
}

function zipFilesAs(entries: [src: string, name: string][], destZip: string) {
	const tmp = destZip + ".tmp_d";
	mkdirSync(tmp, { recursive: true });
	try {
		for (const [src, name] of entries) copyFileSync(src, join(tmp, name));
		zipDir(tmp, destZip);
	} finally {
		rmSync(tmp, { recursive: true, force: true });
	}
}

function cargoVersion(): string {
	const cargo = readFileSync(join(root, "Cargo.toml"), "utf8");
	const m = cargo.match(/^\[package\][^[]*version\s*=\s*"([^"]+)"/ms);
	if (!m) throw new Error("Could not parse version from Cargo.toml");
	return m[1];
}

async function syncPackageJson(version: string) {
	const pkgPath = join(root, "npm/package.json");
	const pkg = await Bun.file(pkgPath).json();
	if (pkg.version === version) return;
	pkg.version = version;
	await Bun.write(pkgPath, JSON.stringify(pkg, null, "\t") + "\n");
	console.log(`[build] synced npm/package.json → ${version}`);
}

function buildTypes() {
	const sdkDir = join(repoRoot, "src/typescript/sdk");
	const luaTypes = join(repoRoot, "src/lua_host/api/types");
	const apiOut = join(sdkDir, "build/api");
	const typesDir = join(root, "npm/bin/millennium/types");

	mkdirSync(typesDir, { recursive: true });

	console.log("[build] compiling TypeScript SDK types");
	run(["bun", "install"], { cwd: sdkDir });
	rmSync(apiOut, { recursive: true, force: true });
	run(["bun", "x", "tsc", "-p", "tsconfig.api.json"], { cwd: sdkDir });

	const apiDts = join(apiOut, "api.d.ts");
	const indexDts = join(apiOut, "index.d.ts");
	if (existsSync(apiDts)) {
		copyFileSync(apiDts, indexDts);
		zipDir(apiOut, join(typesDir, "types.zip"));
		console.log("[build] types.zip");
	}

	const webkitDts = join(apiOut, "millennium-api.d.ts");
	if (existsSync(webkitDts)) {
		const dts = readFileSync(webkitDts, "utf8");
		const names = [...dts.matchAll(/^export\s+(?:declare\s+)?(?:function|const|class|type|enum|interface)\s+(\w+)/mg)].map(m => m[1]);
		const unique = [...new Set(names)].sort();
		writeFileSync(join(typesDir, "webkit-exports.json"), JSON.stringify(unique));

		const tmpIndex = join(apiOut, "_webkit-index.d.ts");
		writeFileSync(tmpIndex, `export * from './millennium-api';\n`);
		zipFilesAs(
			[[webkitDts, "millennium-api.d.ts"], [tmpIndex, "index.d.ts"]],
			join(typesDir, "webkit-types.zip"),
		);
		rmSync(tmpIndex, { force: true });
		console.log("[build] webkit-types.zip + webkit-exports.json");
	}

	if (existsSync(luaTypes)) {
		zipDir(luaTypes, join(typesDir, "lua-types.zip"));
		console.log("[build] lua-types.zip");
	}
}

function build(release: boolean) {
	const args = ["build", ...(release ? ["--release"] : [])];
	console.log(`$ cargo ${args.join(" ")}`);
	run(["cargo", ...args]);
}

function npmPlatform() {
	const osMap: Record<string, string> = { linux: "linux", darwin: "darwin", win32: "win32" };
	const archMap: Record<string, string> = { x64: "x64", arm64: "arm64" };
	const os = osMap[process.platform] ?? process.platform;
	const arch = archMap[process.arch] ?? process.arch;
	const ext = process.platform === "win32" ? ".exe" : "";
	return { os, arch, ext };
}

function copyBinary(release: boolean) {
	const { os, arch, ext } = npmPlatform();
	const profile = release ? "release" : "debug";
	const src = join(root, "target", profile, `starlight${ext}`);
	const destDir = join(root, "npm/binaries");
	const destName = `starlight-${os}-${arch}${ext}`;
	const dest = join(destDir, destName);

	if (!existsSync(src)) {
		console.error(`[build] binary not found at ${src}`);
		process.exit(1);
	}

	mkdirSync(destDir, { recursive: true });
	copyFileSync(src, dest);
	if (process.platform !== "win32") chmodSync(dest, 0o755);
	console.log(`[build] ${src} → npm/binaries/${destName}`);
}

const typesOnly = process.argv.includes("--types-only");
const release = !process.argv.includes("--debug");
const version = cargoVersion();

await syncPackageJson(version);
buildTypes();

if (!typesOnly) {
	build(release);
	copyBinary(release);
}
