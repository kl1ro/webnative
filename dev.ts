import path from "node:path";
import packagePath from "../utils/package-path.js";
import { spawn } from "node:child_process";
import { existsSync } from "node:fs";

export default async function dev() {
  const script = path.join(packagePath, "watch.bash");

  if (!existsSync(script)) {
    throw new Error(`Watch script not found at ${script}`);
  }

  const child = spawn(script, [], {
    stdio: "inherit",
    shell: true,
  });

  return new Promise<void>((resolve, reject) => {
    child.on("error", (err) => {
      reject(new Error(`Failed to start dev server: ${err.message}`));
    });

    child.on("exit", (code) => {
      if (code === 0) resolve();
      else reject(new Error(`Dev server exited with code ${code}`));
    });
  });
}
