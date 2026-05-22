// timers.js — setTimeout and setInterval
//
// Both fire on the main event loop — no threads involved.
// clearInterval stops a repeating timer by the handle returned from setInterval.

console.log("start");

setTimeout(() => {
    console.log("one-shot after 500ms");
}, 500);

let ticks = 0;
const interval = setInterval(() => {
    ticks++;
    console.log(`tick ${ticks}`);
    if (ticks === 3) {
        clearInterval(interval);
        console.log("interval cleared, event loop will drain and exit");
    }
}, 300);
