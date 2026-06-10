import { spawn } from "node:child_process";
import { platform } from "node:os";

const DEFAULT_TIMEOUT_MS = 30 * 60 * 1000; // 30 minutes

export default function exec(
  command: string,
  options?: { env?: NodeJS.ProcessEnv; cwd?: string; ignore?: boolean; timeout?: number },
): Promise<void> {
  return new Promise((resolve, reject) => {
    if (!command || command.trim().length === 0) {
      reject(new Error("Command cannot be empty"));
      return;
    }

    // Better command parsing that handles quoted arguments
    const args = parseCommand(command);
    if (args.length === 0) {
      reject(new Error("Failed to parse command"));
      return;
    }

    const [cmd, ...cmdArgs] = args;
    const child = spawn(cmd, cmdArgs, {
      stdio: options?.ignore ? "ignore" : "inherit",
      env: { ...process.env, ...options?.env },
      cwd: options?.cwd,
      shell: platform() === "win32", // Use shell on Windows
    });

    const timeout = options?.timeout ?? DEFAULT_TIMEOUT_MS;
    let timeoutHandle: NodeJS.Timeout | undefined;
    let completed = false;

    const cleanup = () => {
      if (timeoutHandle) clearTimeout(timeoutHandle);
      process.off("exit", kill);
      process.off("SIGINT", kill);
      process.off("SIGTERM", kill);
    };

    const kill = () => {
      if (!child.killed) child.kill();
    };

    timeoutHandle = setTimeout(() => {
      if (!completed) {
        completed = true;
        kill();
        cleanup();
        reject(new Error(`Command timeout after ${timeout}ms: ${command}`));
      }
    }, timeout);

    process.on("exit", kill);
    process.on("SIGINT", kill);
    process.on("SIGTERM", kill);

    child.on("error", (err) => {
      if (!completed) {
        completed = true;
        cleanup();
        reject(new Error(`Failed to execute command: ${err.message}`));
      }
    });

    child.on("close", (code) => {
      if (!completed) {
        completed = true;
        cleanup();
        if (code === 0) resolve();
        else reject(new Error(`Command failed with exit code ${code}: ${command}`));
      }
    });
  });
}

/**
 * Parse command string respecting quoted arguments
 */
function parseCommand(command: string): string[] {
  const args: string[] = [];
  let current = "";
  let inQuotes = false;
  let quoteChar = "";

  for (let i = 0; i < command.length; i++) {
    const char = command[i];

    if ((char === '"' || char === "'") && (i === 0 || command[i - 1] !== "\\")) {
      if (!inQuotes) {
        inQuotes = true;
        quoteChar = char;
      } else if (char === quoteChar) {
        inQuotes = false;
      } else {
        current += char;
      }
    } else if (char === " " && !inQuotes) {
      if (current.length > 0) {
        args.push(current);
        current = "";
      }
    } else {
      current += char;
    }
  }

  if (current.length > 0) {
    args.push(current);
  }

  return args;
}
