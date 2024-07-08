import {createReadStream, createWriteStream} from "fs"
import {exec} from "child_process"
const pipe = [createReadStream(null, {fd: Number(process.argv[2])}), createWriteStream(null, {fd: Number(process.argv[3])})]
// Uncomment this if you use postgres as I do
// exec("systemctl is-active postgresql.service", (error, stdout) => {
// 	if(stdout.trim() == "active") return
// 	exec("systemctl start postgresql.service")
// })
pipe[0].on("data", async message => {
	const {path, body, reqId} = JSON.parse(message)
	try{pipe[1].write(JSON.stringify({reqId, ...await import("./routes/" + path + ".mjs").then(m => m.default(body))}))}
	catch(e) {pipe[1].write(JSON.stringify({reqId, error: "Route not found"}))}
})