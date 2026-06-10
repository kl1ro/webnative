import { SpecificPlatform } from "../commands/build/builders.js";
import exec from "./exec.js";

export async function checkDocker(platform: SpecificPlatform) {
  try {
    await exec("docker info", { ignore: true });
  } catch (err) {
    throw new Error(
      `Docker is required to build ${platform} target.\n` +
        `Please install Docker: https://docs.docker.com/get-docker/\n` +
        `Error: ${(err as Error).message}`,
    );
  }
}
