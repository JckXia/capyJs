// static_server.js — serving static files
//
// serveStatic() maps a URL prefix to a directory on disk.


const server = new Server();


server.serveStatic("/public", "./public.html");

server.listen(8080, () => {
    console.log("static server on http://localhost:8080");
});
