import { randomBytes, timingSafeEqual } from "crypto";
import { createPipe } from "./api";
import express from "express";
import cors from "cors";

const pipe = await createPipe();
const key = randomBytes(32).toString("hex");

const app = express();
app.use(cors());
app.use(express.json());

app.use((req, res) => {
	const key = req.headers["x-api-key"];

	if (typeof key !== "string" || !checkKey(key)) {
		console.log("Unauthorized: " + req.url);
		return res.status(401).json({ error: "Unauthorized" });
	}

	console.log(`Request authorized: ${req.url}`);
	return res.status(200).json({ status: "authorized" });
});

const server = app.listen(0, "127.0.0.1", () => {
	const port = server.address().port;
	console.log("Serving on port " + port);
	pipe.write(JSON.stringify({ port, key }) + "\0");
});

function checkKey(incoming) {
	const a = Buffer.from(incoming);
	const b = Buffer.from(key);
	if (a.length !== b.length) return false;
	return timingSafeEqual(a, b);
}
