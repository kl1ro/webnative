// This file is compiled into public/index.js by webpack
// It's just an example, you can do with this what you want
import makeApiRequest from "../libs/makeApiRequest"
const h = document.getElementById("h");
(async () => {await makeApiRequest("/api/example").then(({example: t}) => {h.innerText = t})})()