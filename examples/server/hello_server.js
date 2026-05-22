// hello_server.js — minimal HTTP server
//
// Server.get() registers a route handler. The callback receives a Request
// and must return a Response. Handlers are matched in registration order.
// listen() binds the port and hands control to the libuv event loop.

// Life cycle disclaimers:
//   -> Unlike with NodeJS and the V8 engine where values are GC'd. QuickJS relies on manual ref counting.
//      Therefore, the req and res objects are tightly bound together in terms of their life cycles. When res.end() is called
//      both req and res and its underlying Native bindings are destroyed.

//  -> Therefore one must be careful when interacting with other systems (i.e timers):
//          ->  setTimeout(() => {
//                  res.end("delayed")
//               }, 500);   This is safe, because the timer is emphemeral and destroyed alongside res

//          -> setInterval(() => { res.end("interv!")}, 500) is DANGEROUS, bc the timer is re-occuring and will outlive the res object
//          You must clear the timer before sending back a response

const server = new Server();

server.get("/", (req, res) => {
    res.setHeader("Content-Type", "application/plain-text")
        .end("hello world");
});

server.get("/json", (req,res) =>{
    const body = JSON.stringify({runtime: "capy"});
    res.setHeader("Content-Type", "application/json")
      .end(body);
});

server.get("/delay", (req,res) => {
    setTimeout(() => {
        res.end("delayed")
    }, 500);
});

server.get("/interval_safe", (req,res) => {
    ticks = 0
    const interval = setInterval(() => {
        ticks += 1
        if(ticks == 3) {
            clearInterval(interval);  // You MUST clear the interval or it'd result in unsafe memory access
            res.end("Complete!") 
        } else {
            console.log(`tick tock ${ticks}`)
        }
    },500);
});


server.listen(3000, () => {
    console.log("listening on http://localhost:3000");
});
