import path from "node:path";
import { fileURLToPath } from "node:url";

const packagePath = path.dirname(fileURLToPath(import.meta.url));
export default packagePath;
