const server = new Server({staticFiles:[]})


server.get("/uptime", (req,res) => {
  console.log(req.uri)
  res.end("Hello");
});

server.listen(9091, () => {
  console.log(`Server is running on port ${9091}`);
})
