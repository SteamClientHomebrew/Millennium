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

const steps = [
    { name: "ttc", filter: "@steambrew/ttc" },
    { name: "client", filter: "@steambrew/client" },
    { name: "webkit", filter: "@steambrew/webkit" },
    { name: "api", filter: "@steambrew/api" },
    { name: "frontend", filter: "core" },
];

for (const { name, filter } of steps) {
    process.stdout.write(`  building ${name}...`);
    const start = performance.now();
    try {
        await $`bun run --filter ${filter} build`.quiet();
        console.log(
            ` done (${((performance.now() - start) / 1000).toFixed(2)}s)`,
        );
    } catch (e: any) {
        console.log(" failed\n");
        process.stderr.write(e.stderr?.toString() ?? String(e));
        process.exit(1);
    }
}
