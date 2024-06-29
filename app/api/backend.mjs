import {createReadStream, createWriteStream} from "fs"
const pipe = [createReadStream(null, {fd: Number(process.argv[2])}), createWriteStream(null, {fd: Number(process.argv[3])})]
console.log("log from the backend")
pipe[0].on("data", message => {
	console.log(message.toString())
	pipe[1].write("Backend\n")
})