import { mkdir, copyFile, cp } from "node:fs/promises";
import { join } from "node:path";
import cleanup from "../../../utils/cleanup.js";
import packagePath from "../../../utils/package-path.js";
import { cachePath } from "../../../utils/cache-path.js";
import { downloadWindowsNode } from "../../../utils/node.js";
import { zip } from "../../../utils/zip.js";
import findBackend from "../../../utils/backend.js";

const cwd = process.cwd();

const paths = {
  prebuilds: join(packagePath, "prebuilds/dist"),
  node: join(cachePath, "node.exe"),
  tmp: join(cwd, "dist/tmp"),
  public: join(cwd, "app/public"),
  backend: join(cwd, "app/backend/dist"),
  dist: join(cwd, "dist"),
};

export default async function buildWindowsZip() {
  try {
    console.log("Building Windows application...");
    if (await findBackend()) await downloadWindowsNode(paths.node);
    await prepareTmp();
    await mkdir(paths.dist, { recursive: true });
    console.log("Creating Windows zip archive...");
    await zip(paths.tmp, join(paths.dist, "windows.zip"));
    await cleanup();
    console.log("✅ Windows build completed: dist/windows.zip");
  } catch (err) {
    throw new Error(`Windows build failed: ${(err as Error).message}`);
  }
}

async function prepareTmp() {
  await mkdir(paths.tmp, { recursive: true });

  console.log("Copying Windows executable...");
  await copyFile(
    join(paths.prebuilds, "windows.exe"),
    join(paths.tmp, "windows.exe"),
  );

  await copyFile(
    join(paths.prebuilds, "WebView2Loader.dll"),
    join(paths.tmp, "WebView2Loader.dll"),
  );

  await copyFile(
    join(cwd, "webnative.json"),
    join(paths.tmp, "webnative.json"),
  );

  await cp(paths.public, join(paths.tmp, "public"), { recursive: true });

  if (await findBackend()) {
    console.log("Including Node.js backend...");
    await copyFile(paths.node, join(paths.tmp, "node.exe"));
    await cp(paths.backend, join(paths.tmp, "backend"), { recursive: true });
  }
}
