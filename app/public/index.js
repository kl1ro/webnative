/******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	var __webpack_modules__ = ({

/***/ "./libs/makeApiRequest.js":
/*!********************************!*\
  !*** ./libs/makeApiRequest.js ***!
  \********************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (/* binding */ makeApiRequest)
/* harmony export */ });
var _excluded = ["reqId"];
function asyncGeneratorStep(n, t, e, r, o, a, c) { try { var i = n[a](c), u = i.value; } catch (n) { return void e(n); } i.done ? t(u) : Promise.resolve(u).then(r, o); }
function _asyncToGenerator(n) { return function () { var t = this, e = arguments; return new Promise(function (r, o) { var a = n.apply(t, e); function _next(n) { asyncGeneratorStep(a, r, o, _next, _throw, "next", n); } function _throw(n) { asyncGeneratorStep(a, r, o, _next, _throw, "throw", n); } _next(void 0); }); }; }
function _objectWithoutProperties(e, t) { if (null == e) return {}; var o, r, i = _objectWithoutPropertiesLoose(e, t); if (Object.getOwnPropertySymbols) { var n = Object.getOwnPropertySymbols(e); for (r = 0; r < n.length; r++) o = n[r], t.indexOf(o) >= 0 || {}.propertyIsEnumerable.call(e, o) && (i[o] = e[o]); } return i; }
function _objectWithoutPropertiesLoose(r, e) { if (null == r) return {}; var t = {}; for (var n in r) if ({}.hasOwnProperty.call(r, n)) { if (e.indexOf(n) >= 0) continue; t[n] = r[n]; } return t; }
var tasks = {
  ps: {},
  rs: {},
  rjs: {}
};
var [curId, maxId] = [1, 1];
window.receiveSignalFromCpp = function (result) {
  var {
      reqId
    } = result,
    rest = _objectWithoutProperties(result, _excluded);
  var [r, rj] = [tasks.rs[reqId], tasks.rjs[reqId]];
  delete tasks.rs[reqId];
  delete tasks.rjs[reqId];
  delete tasks.ps[reqId];
  if (result.error) rj({
    message: result.error
  });else {
    curId++;
    r(rest);
  }
};
function makeApiRequest(_x) {
  return _makeApiRequest.apply(this, arguments);
}
function _makeApiRequest() {
  _makeApiRequest = _asyncToGenerator(function* (path) {
    var body = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : {};
    while (curId != maxId && tasks.ps[curId]) yield tasks.ps[curId];
    var p = new Promise((r, rj) => {
      window.webkit.messageHandlers.cppSignal.postMessage(JSON.stringify({
        path,
        body,
        reqId: maxId
      }));
      tasks.rs[maxId] = r;
      tasks.rjs[maxId] = rj;
    });
    tasks.ps[maxId++] = p;
    return p;
  });
  return _makeApiRequest.apply(this, arguments);
}

/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		var cachedModule = __webpack_module_cache__[moduleId];
/******/ 		if (cachedModule !== undefined) {
/******/ 			return cachedModule.exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			// no module.id needed
/******/ 			// no module.loaded needed
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId](module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/************************************************************************/
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
var __webpack_exports__ = {};
/*!**********************!*\
  !*** ./app/index.js ***!
  \**********************/
__webpack_require__.r(__webpack_exports__);
/* harmony import */ var _libs_makeApiRequest__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../libs/makeApiRequest */ "./libs/makeApiRequest.js");
function asyncGeneratorStep(n, t, e, r, o, a, c) { try { var i = n[a](c), u = i.value; } catch (n) { return void e(n); } i.done ? t(u) : Promise.resolve(u).then(r, o); }
function _asyncToGenerator(n) { return function () { var t = this, e = arguments; return new Promise(function (r, o) { var a = n.apply(t, e); function _next(n) { asyncGeneratorStep(a, r, o, _next, _throw, "next", n); } function _throw(n) { asyncGeneratorStep(a, r, o, _next, _throw, "throw", n); } _next(void 0); }); }; }

var h = document.getElementById("h");
_asyncToGenerator(function* () {
  yield (0,_libs_makeApiRequest__WEBPACK_IMPORTED_MODULE_0__["default"])("/api/example").then(_ref2 => {
    var {
      example: t
    } = _ref2;
    h.innerText = t;
  });
})();
/******/ })()
;
//# sourceMappingURL=data:application/json;charset=utf-8;base64,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Ozs7O0FDTm1EO0FBQ25ELElBQU1jLENBQUMsR0FBR0MsUUFBUSxDQUFDQyxjQUFjLENBQUMsR0FBRyxDQUFDO0FBQ3RDZixpQkFBQSxDQUFDLGFBQVk7RUFBQyxNQUFNTCxnRUFBYyxDQUFDLGNBQWMsQ0FBQyxDQUFDcUIsSUFBSSxDQUFDQyxLQUFBLElBQWtCO0lBQUEsSUFBakI7TUFBQ0MsT0FBTyxFQUFFQztJQUFDLENBQUMsR0FBQUYsS0FBQTtJQUFNSixDQUFDLENBQUNPLFNBQVMsR0FBR0QsQ0FBQztFQUFBLENBQUMsQ0FBQztBQUFBLENBQUMsRUFBRSxDQUFDLEMiLCJzb3VyY2VzIjpbIndlYnBhY2s6Ly93ZWJuYXRpdmUtdGVzdC1wcm9qZWN0Ly4vbGlicy9tYWtlQXBpUmVxdWVzdC5qcyIsIndlYnBhY2s6Ly93ZWJuYXRpdmUtdGVzdC1wcm9qZWN0L3dlYnBhY2svYm9vdHN0cmFwIiwid2VicGFjazovL3dlYm5hdGl2ZS10ZXN0LXByb2plY3Qvd2VicGFjay9ydW50aW1lL2RlZmluZSBwcm9wZXJ0eSBnZXR0ZXJzIiwid2VicGFjazovL3dlYm5hdGl2ZS10ZXN0LXByb2plY3Qvd2VicGFjay9ydW50aW1lL2hhc093blByb3BlcnR5IHNob3J0aGFuZCIsIndlYnBhY2s6Ly93ZWJuYXRpdmUtdGVzdC1wcm9qZWN0L3dlYnBhY2svcnVudGltZS9tYWtlIG5hbWVzcGFjZSBvYmplY3QiLCJ3ZWJwYWNrOi8vd2VibmF0aXZlLXRlc3QtcHJvamVjdC8uL2FwcC9pbmRleC5qcyJdLCJzb3VyY2VzQ29udGVudCI6WyJjb25zdCB0YXNrcyA9IHtwczoge30sIHJzOiB7fSwgcmpzOiB7fX1cbmxldCBbY3VySWQsIG1heElkXSA9IFsxLCAxXVxud2luZG93LnJlY2VpdmVTaWduYWxGcm9tQ3BwID0gZnVuY3Rpb24ocmVzdWx0KSB7XG5cdGNvbnN0IHtyZXFJZCwgLi4ucmVzdH0gPSByZXN1bHRcblx0Y29uc3QgW3IsIHJqXSA9IFt0YXNrcy5yc1tyZXFJZF0sIHRhc2tzLnJqc1tyZXFJZF1dXG5cdGRlbGV0ZSB0YXNrcy5yc1tyZXFJZF07IGRlbGV0ZSB0YXNrcy5yanNbcmVxSWRdOyBkZWxldGUgdGFza3MucHNbcmVxSWRdXG5cdGlmKHJlc3VsdC5lcnJvcikgcmooe21lc3NhZ2U6IHJlc3VsdC5lcnJvcn0pXG5cdGVsc2Uge2N1cklkKys7IHIocmVzdCl9XG59XG5leHBvcnQgZGVmYXVsdCBhc3luYyBmdW5jdGlvbiBtYWtlQXBpUmVxdWVzdChwYXRoLCBib2R5ID0ge30pIHtcblx0d2hpbGUoY3VySWQgIT0gbWF4SWQgJiYgdGFza3MucHNbY3VySWRdKSBhd2FpdCB0YXNrcy5wc1tjdXJJZF1cblx0Y29uc3QgcCA9IG5ldyBQcm9taXNlKChyLCByaikgPT4ge1xuXHRcdHdpbmRvdy53ZWJraXQubWVzc2FnZUhhbmRsZXJzLmNwcFNpZ25hbC5wb3N0TWVzc2FnZShKU09OLnN0cmluZ2lmeSh7cGF0aCwgYm9keSwgcmVxSWQ6IG1heElkfSkpXG5cdFx0dGFza3MucnNbbWF4SWRdID0gcjsgdGFza3MucmpzW21heElkXSA9IHJqXG5cdH0pXG5cdHRhc2tzLnBzW21heElkKytdID0gcFxuXHRyZXR1cm4gcFxufSIsIi8vIFRoZSBtb2R1bGUgY2FjaGVcbnZhciBfX3dlYnBhY2tfbW9kdWxlX2NhY2hlX18gPSB7fTtcblxuLy8gVGhlIHJlcXVpcmUgZnVuY3Rpb25cbmZ1bmN0aW9uIF9fd2VicGFja19yZXF1aXJlX18obW9kdWxlSWQpIHtcblx0Ly8gQ2hlY2sgaWYgbW9kdWxlIGlzIGluIGNhY2hlXG5cdHZhciBjYWNoZWRNb2R1bGUgPSBfX3dlYnBhY2tfbW9kdWxlX2NhY2hlX19bbW9kdWxlSWRdO1xuXHRpZiAoY2FjaGVkTW9kdWxlICE9PSB1bmRlZmluZWQpIHtcblx0XHRyZXR1cm4gY2FjaGVkTW9kdWxlLmV4cG9ydHM7XG5cdH1cblx0Ly8gQ3JlYXRlIGEgbmV3IG1vZHVsZSAoYW5kIHB1dCBpdCBpbnRvIHRoZSBjYWNoZSlcblx0dmFyIG1vZHVsZSA9IF9fd2VicGFja19tb2R1bGVfY2FjaGVfX1ttb2R1bGVJZF0gPSB7XG5cdFx0Ly8gbm8gbW9kdWxlLmlkIG5lZWRlZFxuXHRcdC8vIG5vIG1vZHVsZS5sb2FkZWQgbmVlZGVkXG5cdFx0ZXhwb3J0czoge31cblx0fTtcblxuXHQvLyBFeGVjdXRlIHRoZSBtb2R1bGUgZnVuY3Rpb25cblx0X193ZWJwYWNrX21vZHVsZXNfX1ttb2R1bGVJZF0obW9kdWxlLCBtb2R1bGUuZXhwb3J0cywgX193ZWJwYWNrX3JlcXVpcmVfXyk7XG5cblx0Ly8gUmV0dXJuIHRoZSBleHBvcnRzIG9mIHRoZSBtb2R1bGVcblx0cmV0dXJuIG1vZHVsZS5leHBvcnRzO1xufVxuXG4iLCIvLyBkZWZpbmUgZ2V0dGVyIGZ1bmN0aW9ucyBmb3IgaGFybW9ueSBleHBvcnRzXG5fX3dlYnBhY2tfcmVxdWlyZV9fLmQgPSAoZXhwb3J0cywgZGVmaW5pdGlvbikgPT4ge1xuXHRmb3IodmFyIGtleSBpbiBkZWZpbml0aW9uKSB7XG5cdFx0aWYoX193ZWJwYWNrX3JlcXVpcmVfXy5vKGRlZmluaXRpb24sIGtleSkgJiYgIV9fd2VicGFja19yZXF1aXJlX18ubyhleHBvcnRzLCBrZXkpKSB7XG5cdFx0XHRPYmplY3QuZGVmaW5lUHJvcGVydHkoZXhwb3J0cywga2V5LCB7IGVudW1lcmFibGU6IHRydWUsIGdldDogZGVmaW5pdGlvbltrZXldIH0pO1xuXHRcdH1cblx0fVxufTsiLCJfX3dlYnBhY2tfcmVxdWlyZV9fLm8gPSAob2JqLCBwcm9wKSA9PiAoT2JqZWN0LnByb3RvdHlwZS5oYXNPd25Qcm9wZXJ0eS5jYWxsKG9iaiwgcHJvcCkpIiwiLy8gZGVmaW5lIF9fZXNNb2R1bGUgb24gZXhwb3J0c1xuX193ZWJwYWNrX3JlcXVpcmVfXy5yID0gKGV4cG9ydHMpID0+IHtcblx0aWYodHlwZW9mIFN5bWJvbCAhPT0gJ3VuZGVmaW5lZCcgJiYgU3ltYm9sLnRvU3RyaW5nVGFnKSB7XG5cdFx0T2JqZWN0LmRlZmluZVByb3BlcnR5KGV4cG9ydHMsIFN5bWJvbC50b1N0cmluZ1RhZywgeyB2YWx1ZTogJ01vZHVsZScgfSk7XG5cdH1cblx0T2JqZWN0LmRlZmluZVByb3BlcnR5KGV4cG9ydHMsICdfX2VzTW9kdWxlJywgeyB2YWx1ZTogdHJ1ZSB9KTtcbn07IiwiaW1wb3J0IG1ha2VBcGlSZXF1ZXN0IGZyb20gXCIuLi9saWJzL21ha2VBcGlSZXF1ZXN0XCJcbmNvbnN0IGggPSBkb2N1bWVudC5nZXRFbGVtZW50QnlJZChcImhcIik7XG4oYXN5bmMgKCkgPT4ge2F3YWl0IG1ha2VBcGlSZXF1ZXN0KFwiL2FwaS9leGFtcGxlXCIpLnRoZW4oKHtleGFtcGxlOiB0fSkgPT4ge2guaW5uZXJUZXh0ID0gdH0pfSkoKSJdLCJuYW1lcyI6WyJ0YXNrcyIsInBzIiwicnMiLCJyanMiLCJjdXJJZCIsIm1heElkIiwid2luZG93IiwicmVjZWl2ZVNpZ25hbEZyb21DcHAiLCJyZXN1bHQiLCJyZXFJZCIsInJlc3QiLCJfb2JqZWN0V2l0aG91dFByb3BlcnRpZXMiLCJfZXhjbHVkZWQiLCJyIiwicmoiLCJlcnJvciIsIm1lc3NhZ2UiLCJtYWtlQXBpUmVxdWVzdCIsIl94IiwiX21ha2VBcGlSZXF1ZXN0IiwiYXBwbHkiLCJhcmd1bWVudHMiLCJfYXN5bmNUb0dlbmVyYXRvciIsInBhdGgiLCJib2R5IiwibGVuZ3RoIiwidW5kZWZpbmVkIiwicCIsIlByb21pc2UiLCJ3ZWJraXQiLCJtZXNzYWdlSGFuZGxlcnMiLCJjcHBTaWduYWwiLCJwb3N0TWVzc2FnZSIsIkpTT04iLCJzdHJpbmdpZnkiLCJoIiwiZG9jdW1lbnQiLCJnZXRFbGVtZW50QnlJZCIsInRoZW4iLCJfcmVmMiIsImV4YW1wbGUiLCJ0IiwiaW5uZXJUZXh0Il0sInNvdXJjZVJvb3QiOiIifQ==