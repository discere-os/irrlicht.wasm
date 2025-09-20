#!/usr/bin/env -S deno run --allow-read --allow-run --allow-write --allow-net

/**
 * Irrlicht.wasm Development Environment
 * Combines file watching and dev server for optimal development experience
 *
 * Usage: deno run --allow-read --allow-run --allow-write --allow-net dev.ts
 */

import { debounce } from "https://deno.land/std@0.208.0/async/debounce.ts";

// Configuration
const PORT = 8080;
const WATCH_PATHS = ["./source/Irrlicht", "./wasm", "./src", "./include"];
const IGNORE_PATTERNS = [/\.git/, /build-/, /install/, /\.o$/, /\.tmp$/];

// State management
let serverProcess: Deno.ChildProcess | null = null;
let isBuilding = false;

// Logging with timestamps
function log(message: string, type: "info" | "success" | "error" | "warning" = "info") {
    const timestamp = new Date().toLocaleTimeString();
    const icons = { info: "🔍", success: "✅", error: "❌", warning: "⚠️" };
    console.log(`[${timestamp}] ${icons[type]} ${message}`);
}

// Build function
async function runBuild(): Promise<boolean> {
    if (isBuilding) {
        log("Build already in progress, skipping...", "warning");
        return false;
    }

    isBuilding = true;
    log("Source files changed - rebuilding...", "info");

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
            log(`Build completed in ${(endTime - startTime).toFixed(0)}ms`, "success");

            // Notify dev server about successful build
            try {
                await fetch(`http://localhost:${PORT}/rebuild`, { method: "POST" });
            } catch {
                // Server might not be running yet
            }

            return true;
        } else {
            log("Build failed", "error");
            console.error(new TextDecoder().decode(stderr));
            return false;
        }
    } catch (error) {
        log(`Build error: ${error}`, "error");
        return false;
    } finally {
        isBuilding = false;
    }
}

// Debounced build to avoid rapid rebuilds
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
            log(`File changed: ${path}`, "info");
            debouncedBuild();
            break;
        }
    }
}

// Start dev server in background
async function startDevServer() {
    log("Starting development server...", "info");

    try {
        serverProcess = new Deno.Command("deno", {
            args: [
                "run",
                "--allow-net",
                "--allow-read",
                "--allow-run",
                "dev-server.ts"
            ],
            stdout: "piped",
            stderr: "piped"
        }).spawn();

        // Wait a moment for server to start
        await new Promise(resolve => setTimeout(resolve, 2000));

        log(`Development server started on http://localhost:${PORT}`, "success");
        log(`Demo available at: http://localhost:${PORT}/demo`, "info");

        return true;
    } catch (error) {
        log(`Failed to start dev server: ${error}`, "error");
        return false;
    }
}

// Start file watcher
async function startFileWatcher() {
    log("Starting file watcher...", "info");
    log(`Watching: ${WATCH_PATHS.join(", ")}`, "info");

    const watcher = Deno.watchFs(WATCH_PATHS, { recursive: true });

    try {
        for await (const event of watcher) {
            if (event.kind === "modify" || event.kind === "create") {
                handleFileChange(event);
            }
        }
    } catch (error) {
        if (error instanceof Deno.errors.Interrupted) {
            log("File watcher stopped", "info");
        } else {
            log(`Watcher error: ${error}`, "error");
        }
    }
}

// Main development environment
async function startDevelopment() {
    console.log("🎮 Irrlicht.wasm Development Environment");
    console.log("=" + "=".repeat(50));
    console.log("");

    // Initial build
    log("Running initial build...", "info");
    const initialBuild = await runBuild();

    if (!initialBuild) {
        log("Initial build failed - fix errors before continuing", "error");
        Deno.exit(1);
    }

    // Start dev server
    const serverStarted = await startDevServer();
    if (!serverStarted) {
        log("Failed to start development server", "error");
        Deno.exit(1);
    }

    console.log("");
    log("Development environment ready!", "success");
    log("Open your browser to start developing", "info");
    console.log("");

    // Start file watcher (this will run until interrupted)
    await startFileWatcher();
}

// Graceful shutdown
async function shutdown() {
    console.log("");
    log("Shutting down development environment...", "info");

    if (serverProcess) {
        try {
            serverProcess.kill("SIGINT");
            await serverProcess.status;
            log("Development server stopped", "info");
        } catch (error) {
            log(`Error stopping server: ${error}`, "warning");
        }
    }

    log("Development environment stopped", "success");
    Deno.exit(0);
}

// Handle shutdown signals
Deno.addSignalListener("SIGINT", shutdown);
Deno.addSignalListener("SIGTERM", shutdown);

// Start development environment
if (import.meta.main) {
    await startDevelopment();
}