import { chmod, mkdir } from "node:fs/promises";
import { existsSync } from "node:fs";
import exec from "./exec.js";
import { dirname } from "node:path";

// AppImage tool release URL
const APPIMAGETOOL_URL =
  "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage";

export async function downloadAppImageTool(path: string) {
  if (existsSync(path)) {
    console.log("AppImageTool already downloaded");
    return;
  }

  console.log("Downloading appimagetool...");
  await mkdir(dirname(path), { recursive: true });

  try {
    await exec(`wget -O ${path} ${APPIMAGETOOL_URL}`);
    await chmod(path, 0o755);
    console.log("AppImageTool downloaded and made executable");
  } catch (err) {
    throw new Error(`Failed to download appimagetool: ${(err as Error).message}`);
  }
}
