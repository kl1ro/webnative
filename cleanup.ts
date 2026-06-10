import { rm, access } from "node:fs/promises";
import { join } from "node:path";
import { cwd } from "node:process";

const buildPath = join(cwd(), "dist");
const tmpPath = join(buildPath, "tmp");

export default async function cleanup() {
  try {
    // Check if tmp directory exists before trying to remove it
    try {
      await access(tmpPath);
    } catch {
      // Directory doesn't exist, nothing to clean
      return;
    }

    console.log("Cleaning up temporary build files...");
    await rm(tmpPath, { recursive: true, force: true });
    console.log("Cleanup completed successfully");
  } catch (err) {
    throw new Error(`Failed to clean up build artifacts: ${(err as Error).message}`);
  }
}
