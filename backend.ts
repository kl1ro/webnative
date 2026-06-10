import { access } from "node:fs/promises";
import { join } from "node:path";
import { cwd } from "node:process";

let backendFound: boolean | undefined = undefined;

export default async function findBackend() {
  // Return cached result if available
  if (backendFound !== undefined) return backendFound;

  try {
    const backendPath = join(cwd(), "app/backend/dist/index.js");
    await access(backendPath);
    backendFound = true;
  } catch (err) {
    // Backend not found - this is not necessarily an error
    console.debug(
      `Backend not found in app/backend/dist/index.js. Running in frontend-only mode.`,
    );
    backendFound = false;
  }

  return backendFound;
}
