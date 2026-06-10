import { mkdir, copyFile, cp, writeFile, chmod } from "node:fs/promises";
import { join } from "node:path";
import exec from "../../../utils/exec.js";
import cleanup from "../../../utils/cleanup.js";
import packagePath from "../../../utils/package-path.js";
import { downloadAppImageTool } from "../../../utils/appimagetool.js";
import { cachePath } from "../../../utils/cache-path.js";
import { getConfig } from "../../../utils/config.js";
import { downloadLinuxNode } from "../../../utils/node.js";
import findBackend from "../../../utils/backend.js";
import { checkDocker } from "../../../utils/docker.js";

const cwd = process.cwd();

const paths = {
  prebuilds: join(packagePath, "prebuilds/dist"),
  appImageTool: join(cachePath, "appimagetool"),
  node: join(cachePath, "node"),
  bin: join(cwd, "dist/tmp/bin"),
  public: join(cwd, "app/public"),
  backend: join(cwd, "app/backend/dist"),
  dist: join(cwd, "dist"),
};

export default function buildLinuxAppImage() {
  switch (process.platform) {
    case "win32":
      return buildLinuxAppImageOnWindows();
    default:
      return buildLinuxAppImageOnLinux();
  }
}

async function buildLinuxAppImageOnWindows() {
  console.log("Building Linux AppImage via Docker...");
  await checkDocker("linux");

  return exec(
    `docker run --rm -v "${cwd}:/app" -w /app node:20-slim npx @mindw1n/webnative@latest build linux`,
  );
}

async function buildLinuxAppImageOnLinux() {
  try {
    console.log("Building Linux AppImage...");
    await findBackend();
    await downloadAppImageTool(paths.appImageTool);

    if (await findBackend()) await downloadLinuxNode(paths.node);

    await prepareBin();
    await mkdir(paths.dist, { recursive: true });

    console.log("Creating AppImage...");
    await exec(
      `${paths.appImageTool} ${paths.bin} -n ${paths.dist}/linux-appimage`,
    );

    await chmod(join(paths.dist, "linux-appimage"), 0o755);
    await cleanup();
    console.log("✅ Linux build completed: dist/linux-appimage");
  } catch (err) {
    throw new Error(`Linux build failed: ${(err as Error).message}`);
  }
}

async function prepareBin() {
  const config = await getConfig();

  await mkdir(join(paths.bin, "usr/bin"), { recursive: true });
  await copyFile(join(paths.prebuilds, "AppRun"), join(paths.bin, "AppRun"));
  await chmod(join(paths.bin, "AppRun"), 0o755);

  const categories = (config.categories ?? ["Utility"]).join(";");
  await writeFile(
    join(paths.bin, "app.desktop"),
    `[Desktop Entry]\nName=${config.name}\nExec=app\nIcon=app\nType=Application\nCategories=${categories};`,
  );

  if (config.icon)
    await copyFile(join(cwd, config.icon), join(paths.bin, "app.png"));

  await copyFile(
    join(paths.prebuilds, "linux"),
    join(paths.bin, "usr/bin/linux"),
  );

  await copyFile(
    join(cwd, "webnative.json"),
    join(paths.bin, "usr/bin/webnative.json"),
  );

  await chmod(join(paths.bin, "usr/bin/linux"), 0o755);

  await cp(paths.public, join(paths.bin, "usr/bin/public"), {
    recursive: true,
  });

  if (await findBackend()) {
    console.log("Including Node.js backend...");
    await cp(paths.backend, join(paths.bin, "usr/bin/backend"), {
      recursive: true,
    });

    await copyFile(paths.node, join(paths.bin, "usr/bin/node"));
    await chmod(join(paths.bin, "usr/bin/node"), 0o755);
  }
}
