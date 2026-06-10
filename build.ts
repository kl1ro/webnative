import { readFile } from "node:fs/promises";
import {
  builders,
  getPlatformsDesc,
  Platform,
  SpecificPlatform,
  SpecificTarget,
  Target,
} from "./builders.js";
import { Command } from "commander";
import { join } from "node:path";
import { PackageJson } from "package-json";
import exec from "../../utils/exec.js";
import findBackend from "../../utils/backend.js";

export function registerBuild(program: Command) {
  program
    .command("build <platform>")
    .usage("<platform> [options]")
    .description(`Build the application\n\n${getPlatformsDesc()}`)
    .option("-t, --target <target>", "build specific target", "all")
    .addHelpText(
      "after",
      `\nExamples:\n  $ webnative build linux\n  $ webnative build linux --target appimage\n  $ webnative build linux -t appimage`,
    )
    .action(async (platform: Platform, options: BuildOptions) =>
      build(platform, options.target),
    );
}

interface BuildOptions {
  target: Target;
}

export default async function build(platform: Platform, target: Target) {
  if (platform == "all") {
    if (target != "all")
      throw new Error("Can't use target option with platform set to 'all'");

    return buildAllPlatforms();
  }

  return buildSpecificPlatform(platform, target);
}

export async function buildSpecificPlatform(
  platform: SpecificPlatform,
  target: Target,
) {
  if (target == "all") return buildAllTargetsOfSpecificPlatform(platform);
  return buildSpecificTarget(platform, target);
}

export async function buildAllTargetsOfSpecificPlatform(
  platform: SpecificPlatform,
) {
  for (const target of Object.keys(builders[platform]) as SpecificTarget[])
    await buildSpecificTarget(platform, target);
}

export async function buildSpecificTarget(
  platform: SpecificPlatform,
  target: SpecificTarget,
) {
  const availableTargets = builders[platform] as Record<
    string,
    () => Promise<void>
  >;

  const build = availableTargets[target];

  if (!build)
    throw new Error(`Unknown target ${target}\n\n${getPlatformsDesc()}`);

  await buildFullstack(platform);

  const found = await findBackend();

  if (!found)
    console.log("Backend not found, Node.js will not be in the output");

  console.log(`Building ${platform} ${target}...`);
  await build();
  console.log(`${target} is built`);
}

export async function buildFullstack(platform: SpecificPlatform) {
  console.log(`Building fullstack for ${platform}...`);

  const pkg = JSON.parse(
    await readFile(join(process.cwd(), "package.json"), "utf-8"),
  ) as PackageJson;

  if (!pkg.scripts?.build) return;

  await exec("npm run build", {
    env: { ...process.env, PLATFORM: platform },
  });
}

export async function buildAllPlatforms() {
  for (const platform of Object.keys(builders) as SpecificPlatform[])
    await buildSpecificPlatform(platform, "all");
}
