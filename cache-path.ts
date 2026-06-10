import { join } from "node:path";
import { homedir } from "node:os";

export const cachePath = join(homedir(), ".webnative");
