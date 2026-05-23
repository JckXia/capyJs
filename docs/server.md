# Server

HTTP server with route registration and static file serving.

## Constructor

```js
const server = new Server();
```

## Methods

### server.get(path, handler)

Registers a GET route. Handlers are matched in registration order.

```js
server.get("/hello", (req, res) => {
    res.setHeader("Content-Type", "text/plain").end("hello");
});
```

**req properties:**
- `req.url` — request path string
- `req.method` — HTTP verb string

**res methods:**
- `res.setHeader(key, value)` — sets a response header, returns `res` for chaining
- `res.end(body)` — sends the response and destroys req/res. Must be called exactly once.

### server.serveStatic(urlPrefix, path)

Serves files from a local path under a URL prefix.

```js
server.serveStatic("/public", "./public.html");
```

### server.ws(path, handlers)

Registers a WebSocket upgrade handler.

```js
server.ws("/chat", {
    open(ws) { ws.send("connected"); },
    message(ws, msg) { ws.send(msg); },
    close(ws) { },
});
```

**ws methods:**
- `ws.send(message)` — sends a text frame to the client

### server.listen(port, callback)

Binds the port and starts the event loop.

```js
server.listen(3000, () => {
    console.log("listening on :3000");
});
```

## Lifecycle

`req` and `res` are backed by native objects tied to the underlying TCP connection.
Their lifetime ends when `res.end()` is called. Holding references past that point
is undefined behavior.

```js
// safe — timer is ephemeral, fires once before res dies
server.get("/delay", (req, res) => {
    setTimeout(() => res.end("delayed"), 500);
});

// unsafe — interval outlives res after end() is called
server.get("/bad", (req, res) => {
    const t = setInterval(() => {
        res.end("tick");       // res is destroyed after first call
        clearInterval(t);      // too late
    }, 100);
});

// safe — clear the interval before calling end()
server.get("/safe", (req, res) => {
    let ticks = 0;
    const t = setInterval(() => {
        if (++ticks === 3) {
            clearInterval(t);
            res.end("done");
        }
    }, 100);
});
```
