// memory/capy_server.js
// Measure RSS under sustained load.
// QuickJS has a significantly smaller baseline footprint than V8.
//
// Measure with: ps -o rss= -p <pid>  (or use the run.sh script)

const server = new Server();

server.get("/", (req, res) => {
    res.setHeader("Content-Type", "application/json")
        .end(JSON.stringify({ ok: true, ts: Date.now() }));
});

server.listen(3001, () => {
    console.log("capy memory on :3001  pid=" + (typeof process !== "undefined" ? process.pid : "?"));
});
