import { program } from "commander";
import { registerBuild } from "./commands/build/build.js";
import { registerInit } from "./commands/init.js";

registerInit(program);
registerBuild(program);
program.parse();
