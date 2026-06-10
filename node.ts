import { existsSync } from "node:fs";
import download from "./download.js";
import exec from "./exec.js";
import { dirname } from "node:path";
import { chmod, mkdir, rm, access } from "node:fs/promises";
import { extractFileFromZip } from "./zip.js";

// Node.js version to download
const NODE_VERSION = "v22.0.0";

export async function downloadLinuxNode(outputPath: string) {
  if (existsSync(outputPath)) {
    // Verify it's executable
    try {
      await access(outputPath);
      return;
    } catch {
      console.warn(`Node binary at ${outputPath} exists but is not accessible. Redownloading...`);
    }
  }

  console.log(`Downloading Node.js ${NODE_VERSION} for Linux...`);
  await mkdir(dirname(outputPath), { recursive: true });

  const url = `https://nodejs.org/dist/${NODE_VERSION}/node-${NODE_VERSION}-linux-x64.tar.gz`;
  const tarPath = outputPath + ".tar.gz";

  try {
    await download(url, tarPath);
    console.log("Extracting Node.js archive...");

    await exec(
      `tar -xz -f ${tarPath} -C ${dirname(outputPath)} --strip-components=2 node-${NODE_VERSION}-linux-x64/bin/node`,
    );

    // Verify extraction
    if (!existsSync(outputPath)) {
      throw new Error("Node binary was not extracted successfully");
    }

    await chmod(outputPath, 0o755);
    console.log("Node.js downloaded and installed successfully");
  } catch (err) {
    throw new Error(`Failed to download/install Node.js for Linux: ${(err as Error).message}`);
  } finally {
    // Cleanup tar file
    try {
      if (existsSync(tarPath)) await rm(tarPath);
    } catch {
      // Silent fail on cleanup
    }
  }
}

export async function downloadWindowsNode(outputPath: string) {
  if (existsSync(outputPath)) {
    // Verify it's accessible
    try {
      await access(outputPath);
      return;
    } catch {
      console.warn(`Node binary at ${outputPath} exists but is not accessible. Redownloading...`);
    }
  }

  console.log(`Downloading Node.js ${NODE_VERSION} for Windows...`);
  await mkdir(dirname(outputPath), { recursive: true });

  const url = `https://nodejs.org/dist/${NODE_VERSION}/node-${NODE_VERSION}-win-x64.zip`;
  const zipPath = outputPath + ".zip";

  try {
    await download(url, zipPath);
    console.log("Extracting Node.js archive...");

    await extractFileFromZip(
      zipPath,
      `node-${NODE_VERSION}-win-x64/node.exe`,
      outputPath,
    );

    // Verify extraction
    if (!existsSync(outputPath)) {
      throw new Error("Node binary was not extracted successfully");
    }

    console.log("Node.js downloaded and installed successfully");
  } catch (err) {
    throw new Error(`Failed to download/install Node.js for Windows: ${(err as Error).message}`);
  } finally {
    // Cleanup zip file
    try {
      if (existsSync(zipPath)) await rm(zipPath);
    } catch {
      // Silent fail on cleanup
    }
  }
}
