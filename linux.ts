import { createWriteStream } from "fs";

export async function createPipe() {
	return createWriteStream("", { fd: Number(process.argv[2]) });
}
