const server = new Server({staticFiles:[]})


server.serveStatic("/index.html", "./server.js");

server.get("/uptime", (req,res) => {
  res.end("Hello");
});

server.listen(9091, () => {
  console.log("Server is running");
})
