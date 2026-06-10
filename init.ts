import { Command } from "commander";
import packagePath from "../utils/package-path.js";
import { join } from "node:path";
import { cp, readFile, writeFile, mkdir } from "node:fs/promises";
import exec from "../utils/exec.js";
import { createInterface } from "node:readline/promises";
import { PackageJson } from "package-json";
import { existsSync } from "node:fs";

export function registerInit(program: Command) {
  program
    .command("init [project-name]")
    .description("Initialize a new webnative project")
    .addHelpText(
      "after",
      `\nExamples:\n  $ webnative init\n  $ webnative init my-app`,
    )
    .action(init);
}

export default async function init(name?: string) {
  const targetPath = name ? join(process.cwd(), name) : process.cwd();

  // Validate target path
  if (name && existsSync(targetPath)) {
    throw new Error(`Directory ${name} already exists`);
  }

  console.log(`Initializing webnative project${name ? ` in ${name}` : ""}...`);
  const typescript = await askTypeScript();
  await copyTemplateToPackage(typescript, targetPath);
  await prepareNodeEnvironment(typescript, targetPath);
  console.log(
    `\n✅ Project initialized successfully${name ? ` in ${name}` : ""}\n`,
  );
  console.log("Next steps:");
  if (name) console.log(`  cd ${name}`);
  console.log("  webnative dev (to start development)");
  console.log("  webnative build linux (to build for Linux)");
}

async function askTypeScript() {
  const rl = createInterface({ input: process.stdin, output: process.stdout });
  try {
    const answer = await rl.question(
      "Would you like to use TypeScript? (y/n) [y]: ",
    );
    return (answer.toLowerCase() === "y" || answer === "");
  } finally {
    rl.close();
  }
}

export async function copyTemplateToPackage(
  typescript: boolean,
  destination: string,
) {
  try {
    await mkdir(destination, { recursive: true });
    await cp(
      join(packagePath, "template", typescript ? "ts" : "js"),
      destination,
      { recursive: true },
    );
  } catch (err) {
    throw new Error(
      `Failed to copy template: ${(err as Error).message}`,
    );
  }
}

async function prepareNodeEnvironment(
  typescript: boolean,
  targetPath: string,
) {
  try {
    console.log("Installing dependencies...");
    await exec("npm init -y", { cwd: targetPath });
    const pkgPath = join(targetPath, "package.json");
    await preparePackageJson(typescript, pkgPath);
    await installDependencies(typescript, targetPath);
  } catch (err) {
    throw new Error(
      `Failed to prepare Node environment: ${(err as Error).message}`,
    );
  }
}

async function preparePackageJson(
  typescript: boolean,
  pkgPath: string,
) {
  try {
    const pkg = JSON.parse(await readFile(pkgPath, "utf-8")) as PackageJson;

    pkg.scripts = {
      ...pkg.scripts,
      build: "webpack",
    };

    pkg.type = "module";
    await writeFile(pkgPath, JSON.stringify(pkg, null, 2), "utf-8");
  } catch (err) {
    throw new Error(
      `Failed to update package.json: ${(err as Error).message}`,
    );
  }
}

async function installDependencies(
  typescript: boolean,
  targetPath: string,
) {
  const deps = `webpack webpack-cli @mindw1n/webnative-core cors express${
    typescript
      ? " ts-loader typescript @types/cors @types/express"
      : ""
  }`;

  try {
    await exec(`npm install ${deps} --save-dev`, { cwd: targetPath });
  } catch (err) {
    throw new Error(
      `Failed to install dependencies: ${(err as Error).message}`,
    );
  }
}
