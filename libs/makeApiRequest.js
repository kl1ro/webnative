const rs = {}; const rjs = {}
let id = 1
window.receiveSignalFromCpp = function(result) {
	const {reqId, ...rest} = result
	const [r, rj] = [rs[reqId], rjs[reqId]]
	delete rs[reqId]; delete rjs[reqId]
	if(result.error) rj({message: result.error})
	r(rest)
}
export default function makeApiRequest(path, body = {}) {
	return new Promise((r, rj) => {
		window.webkit.messageHandlers.cppSignal.postMessage(JSON.stringify({path, body, reqId: id}))
		rs[id] = r; rjs[id++] = rj
	})
}