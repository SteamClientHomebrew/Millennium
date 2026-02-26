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
