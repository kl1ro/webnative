const rs = {}
let id = 1
window.receiveSignalFromCpp = function(result) {
	const {reqId, ...rest} = result
	const r = rs[reqId]
	delete rs[reqId]
	r(rest)
}
export default function makeApiRequest(path, body = {}) {
	return new Promise(r => {
		window.webkit.messageHandlers.cppSignal.postMessage(JSON.stringify({path, body, reqId: id}))
		rs[id++] = r
	})
}