#!/usr/bin/env -S deno run --allow-read --allow-run --allow-write

/**
 * Development File Watcher for Irrlicht.wasm
 * Monitors source files and triggers automatic rebuilds
 *
 * Usage: deno run --allow-read --allow-run --allow-write dev-watch.ts
 */

import { debounce } from "https://deno.land/std@0.208.0/async/debounce.ts";

// Configuration
const WATCH_PATHS = [
    "./source/Irrlicht",
    "./wasm",
    "./src",
    "./include"
];

const IGNORE_PATTERNS = [
    /\.git/,
    /build-/,
    /install/,
    /node_modules/,
    /\.o$/,
    /\.tmp$/
];

// Build command
async function runBuild(): Promise<boolean> {
    console.log("🔄 Source files changed - rebuilding...");

    const startTime = performance.now();

    try {
        const buildProcess = new Deno.Command("./build-dual.sh", {
            args: ["main"],
            stdout: "piped",
            stderr: "piped"
        });

        const { code, stdout, stderr } = await buildProcess.output();
        const endTime = performance.now();

        if (code === 0) {
            console.log(`✅ Build completed in ${(endTime - startTime).toFixed(0)}ms`);

            // Show build output
            const buildOutput = new TextDecoder().decode(stdout);
            const buildLines = buildOutput.split('\n').filter(line =>
                line.includes('SUCCESS') || line.includes('INFO')
            );
            buildLines.forEach(line => console.log(`   ${line}`));

            return true;
        } else {
            console.error("❌ Build failed:");
            console.error(new TextDecoder().decode(stderr));
            return false;
        }
    } catch (error) {
        console.error("❌ Build error:", error);
        return false;
    }
}

// Debounced build function (avoid rapid rebuilds)
const debouncedBuild = debounce(runBuild, 500);

// File change handler
function handleFileChange(event: Deno.FsEvent) {
    for (const path of event.paths) {
        // Skip ignored patterns
        if (IGNORE_PATTERNS.some(pattern => pattern.test(path))) {
            continue;
        }

        // Only rebuild for relevant file types
        if (path.match(/\.(cpp|h|c|ts|js)$/)) {
            console.log(`📁 Changed: ${path}`);
            debouncedBuild();
            break;
        }
    }
}

// Initialize file watcher
async function startWatcher() {
    console.log("🎮 Irrlicht.wasm Development Watcher");
    console.log("=" + "=".repeat(40));
    console.log("");
    console.log("📁 Watching paths:");
    WATCH_PATHS.forEach(path => console.log(`   ${path}`));
    console.log("");
    console.log("🔄 Auto-rebuild triggers on: .cpp, .h, .c, .ts, .js files");
    console.log("⚡ Debounced rebuild: 500ms delay");
    console.log("");

    // Initial build
    console.log("🚀 Running initial build...");
    const initialBuildSuccess = await runBuild();

    if (!initialBuildSuccess) {
        console.error("❌ Initial build failed - fix errors before starting watcher");
        Deno.exit(1);
    }

    console.log("");
    console.log("👀 Watching for changes... (Ctrl+C to stop)");
    console.log("");

    // Start file system watcher
    const watcher = Deno.watchFs(WATCH_PATHS, { recursive: true });

    try {
        for await (const event of watcher) {
            if (event.kind === "modify" || event.kind === "create") {
                handleFileChange(event);
            }
        }
    } catch (error) {
        if (error instanceof Deno.errors.Interrupted) {
            console.log("");
            console.log("🛑 File watcher stopped");
        } else {
            console.error("❌ Watcher error:", error);
        }
    }
}

// Graceful shutdown
Deno.addSignalListener("SIGINT", () => {
    console.log("");
    console.log("🛑 Shutting down development watcher...");
    Deno.exit(0);
});

// Start the watcher
if (import.meta.main) {
    await startWatcher();
}