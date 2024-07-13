const tasks = {ps: {}, rs: {}, rjs: {}}
let [curId, maxId] = [1, 1]
window.receiveSignalFromCpp = function(result) {
	const {reqId, ...rest} = result
	const [r, rj] = [tasks.rs[reqId], tasks.rjs[reqId]]
	delete tasks.rs[reqId]; delete tasks.rjs[reqId]; delete tasks.ps[reqId]
	if(result.error) rj({message: result.error})
	else {curId++; r(rest)}
}
export default async function makeApiRequest(path, body = {}) {
	while(curId != maxId && tasks.ps[curId]) await tasks.ps[curId]
	const p = new Promise((r, rj) => {
		window.webkit.messageHandlers.cppSignal.postMessage(JSON.stringify({path, body, reqId: maxId}))
		tasks.rs[maxId] = r; tasks.rjs[maxId] = rj
	})
	tasks.ps[maxId++] = p
	return p
}