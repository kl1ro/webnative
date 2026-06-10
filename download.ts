import { createWriteStream } from "node:fs";
import { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";
import { access } from "node:fs/promises";
import { dirname } from "node:path";
import { mkdir } from "node:fs/promises";

const DEFAULT_TIMEOUT_MS = 5 * 60 * 1000; // 5 minutes
const MAX_RETRIES = 3;

export default async function download(
  url: string,
  outputPath: string,
  options?: { timeout?: number; retries?: number },
) {
  if (!url || url.trim().length === 0) {
    throw new Error("Download URL cannot be empty");
  }

  if (!outputPath || outputPath.trim().length === 0) {
    throw new Error("Output path cannot be empty");
  }

  const timeout = options?.timeout ?? DEFAULT_TIMEOUT_MS;
  const maxRetries = options?.retries ?? MAX_RETRIES;

  // Ensure output directory exists
  const outputDir = dirname(outputPath);
  try {
    await mkdir(outputDir, { recursive: true });
  } catch (err) {
    throw new Error(`Failed to create output directory ${outputDir}: ${(err as Error).message}`);
  }

  // Try download with retries
  let lastError: Error | null = null;
  for (let attempt = 1; attempt <= maxRetries; attempt++) {
    try {
      await downloadWithTimeout(url, outputPath, timeout);
      return;
    } catch (err) {
      lastError = err as Error;
      if (attempt < maxRetries) {
        const backoffMs = Math.min(1000 * Math.pow(2, attempt - 1), 10000);
        console.log(`Download attempt ${attempt} failed. Retrying in ${backoffMs}ms...`);
        await new Promise((resolve) => setTimeout(resolve, backoffMs));
      }
    }
  }

  throw new Error(`Failed to download ${url} after ${maxRetries} attempts: ${lastError?.message}`);
}

async function downloadWithTimeout(url: string, outputPath: string, timeoutMs: number) {
  const controller = new AbortController();
  const timeoutHandle = setTimeout(() => controller.abort(), timeoutMs);

  try {
    const response = await fetch(url, { signal: controller.signal });

    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }

    if (!response.body) {
      throw new Error("Response body is empty");
    }

    await pipeline(
      Readable.fromWeb(response.body as Parameters<typeof Readable.fromWeb>[0]),
      createWriteStream(outputPath),
    );
  } catch (err) {
    if (err instanceof Error && err.name === "AbortError") {
      throw new Error(`Download timeout after ${timeoutMs}ms`);
    }
    throw err;
  } finally {
    clearTimeout(timeoutHandle);
  }
}
