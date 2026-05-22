// memory/node_server.js
// Measure RSS under sustained load.
// V8 baseline footprint is substantially larger than QuickJS.
//
// Measure with: ps -o rss= -p <pid>  (or use the run.sh script)

const http = require("http");

http.createServer((req, res) => {
    res.writeHead(200, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ ok: true, ts: Date.now() }));
}).listen(3000, () => {
    console.log(`node memory on :3000  pid=${process.pid}`);
});
