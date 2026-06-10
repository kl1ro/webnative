import { readFile } from "node:fs/promises";
import { join } from "node:path";
import { existsSync } from "node:fs";

let config: WebnativeConfig | undefined = undefined;

interface WebnativeConfig {
  id: `${string}.${string}.${string}`;
  name: string;
  icon?: string;
  categories?: string[];
}

export async function getConfig(configPath?: string) {
  if (config) return config;
  config = await readConfig(configPath);
  return config;
}

async function readConfig(configPath?: string) {
  const path = configPath || join(process.cwd(), "webnative.json");

  if (!existsSync(path)) {
    throw new Error(`Config file not found: ${path}`);
  }

  try {
    const raw = await readFile(path, "utf-8");
    const parsed = JSON.parse(raw) as WebnativeConfig;

    // Validate required fields
    if (!parsed.id || !parsed.name) {
      throw new Error("Config must contain 'id' and 'name' fields");
    }

    // Validate id format (should be reverse domain notation)
    if (!/^[a-z0-9.-]+$/.test(parsed.id)) {
      throw new Error(`Invalid config id format: ${parsed.id}`);
    }

    return parsed;
  } catch (err) {
    if (err instanceof SyntaxError) {
      throw new Error(`Invalid JSON in config file: ${err.message}`);
    }
    throw err;
  }
}
