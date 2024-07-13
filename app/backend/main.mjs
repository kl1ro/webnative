import {createReadStream, createWriteStream} from "fs"
import ApiError from "./libs/ApiError.mjs"
const pipe = [createReadStream(null, {fd: Number(process.argv[2])}), createWriteStream(null, {fd: Number(process.argv[3])})]
let [chunk, buffer] = [0]
pipe[0].on("data", async message => {
	try{
		if(message == "more") {
			pipe[1].write(buffer.subarray(chunk * 65535, (chunk + 1) * 65535))
			if((chunk + 1) * 65535 >= buffer.length) chunk = 0
			else chunk++
			return
		}
		let {path, body, reqId} = JSON.parse(message)
		if(path[0] == "/") path = "./" + path.slice(1) + ".mjs"
		else {pipe[1].write(JSON.stringify({reqId, error: "Route not found"})); return}
		buffer = Buffer.from(JSON.stringify({reqId, ...await import(path).then(m => m.default(body))}), "utf-8")
		if(buffer.length > 65535) {pipe[1].write(buffer.subarray(chunk * 65535, (chunk + 1) * 65535)); chunk++; return}
		pipe[1].write(buffer)
	}
	catch(e) {
		if(e.name == "PrismaClientInitializationError") pipe[1].write(JSON.stringify({reqId, error: "You need to start the postgres service"}))
		else if(e instanceof ApiError) pipe[1].write(JSON.stringify({reqId, error: e.message}))
		else pipe[1].write(JSON.stringify({reqId, error: "Route not found"}))
	}
})