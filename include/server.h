
#pragma once

#include "client_op.h"
#include "mem_pool.h"
#include "runtime_context.h"
#include "uv.h"
#include <cstring>
#include <functional>
#include <map>

// Below can probably become variable params/config object
#define N_BACKLOG 10
#define MEM_POOL_SIZE 10000
#define READ_BUFFER_POOL_SIZE 200

using namespace std; // get rid of this when we add an actual logger to the
                     // server
// Client state can probably be separated into their own class
// "GET" "POST" "DELETE" "PATCH"

// We might move to an approach that registers URL mapping against function
// handler (i.e express' app.get("/get", func(){}) style) This should be enough
// for embedding with V8 for now. It encapsualtes a lot of the libuv plumbing
// under the hood Actually, let's just do it right and get it over with
class Server {
public:
  explicit Server(int portNum, RuntimeContext *env);
  explicit Server(int portNum, const char *portAddr); // More options later
  void registerFuncHandler(const char *method, const char *uri,
                           Handler handler);

  int run();
  int listen();

private:
  RuntimeContext *_env;
  int portNum;
  const char *portAddr;

  // libuv life cycle functions:
  static void on_write_cb(uv_write_t *req, int status);
  static void on_read_cb(uv_stream_t *client, ssize_t nread,
                         const uv_buf_t *buf);
  static void on_alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size,
                                 uv_buf_t *buf);
  static void on_client_closed_cb(uv_handle_t *handle);
  static void on_signal(uv_signal_t *handle, int signum);
  static void on_peer_connected(uv_stream_t *server_stream, int status);
  static void on_client_closed_emergency(uv_handle_t *handle);

  static void init_client_socket(ClientState *client_state);
};

void Server::init_client_socket(ClientState *client_state) {
  uv_tcp_t *client_sock = &client_state->socket;
  client_sock->data = client_state;
  int rc;
  if ((rc = uv_tcp_init(uv_default_loop(), client_sock)) <
      0) { // create an fd mapped to the client for the kernel
    std::cout << " UV TCP INIT FAILED " << uv_strerror(rc) << std::endl;
    return;
  }
}

void Server::on_client_closed_emergency(uv_handle_t *handle) {
  RuntimeContext *ctx = (RuntimeContext *)handle->loop->data;
  ctx->http_ctx->release_emergency_handle((uv_tcp_t *)handle);
}

void Server::on_client_closed_cb(uv_handle_t *handle) {
  uv_tcp_t *client_sock = (uv_tcp_t *)handle;
  ClientState *client = (ClientState *)client_sock->data;
  RuntimeContext *ctx = (RuntimeContext *)handle->loop->data;
  ctx->http_ctx->release_connection(client);
}

void Server::on_write_cb(uv_write_t *req, int status) {
  if (status) {
    std::cout << "ERROR! " << uv_strerror(status) << std::endl;
  } else {
    // std::cout<<"Write completed \n";
  }
  ClientState *client_state = (ClientState *)req->data;
  client_state->write_in_flight = false;
  uv_read_start((uv_stream_t *)&client_state->socket, on_alloc_buffer_cb,
                on_read_cb);
}

void Server::on_read_cb(uv_stream_t *client, ssize_t nread,
                        const uv_buf_t *buf) {
  ClientState *client_state = (ClientState *)client->data;
  RuntimeContext *ctx = (RuntimeContext *)client->loop->data;

  if (nread > 0) {

    ReadBuffer *rb = (ReadBuffer *)buf->base;
    rb->len = nread;

    if (client_state->write_in_flight) {
      ctx->http_ctx->release_read_buffer(rb);

      uv_read_stop(client);
      return;
    }

    client_state->write_in_flight = true;
    client_ops::recv_new_buffer(client_state, rb);

    RequestObject req;
    ResponseObject res;
    client_ops::populate_request_object(client_state, req);

    client_ops::clear_buffer(client_state, ctx);
    ctx->http_ctx->invoke_function(req, res, req.verb, req.uri);

    uv_buf_t buff = uv_buf_init((char *)res.response, res.response_len);
    uv_write_t *write_handle = &client_state->write_handle;
    write_handle->data = client_state;
    int rc;
    if ((rc = uv_write(write_handle, client, &buff, 1, on_write_cb)) < 0) {
      std::cout << "Write to socket failed! " << uv_strerror(rc) << std::endl;
    }
  } else if (nread == UV_EOF) {

    ctx->http_ctx->release_read_buffer((ReadBuffer *)buf->base);
    //   client_state->global_read_buffer->release((ReadBuffer *)buf->base);
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else if (nread == UV_ENOBUFS) {
    std::cout << "Throttling requests because pool ran out! \n";

    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else {
    ctx->http_ctx->release_read_buffer((ReadBuffer *)buf->base);

    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  }
}

void Server::on_alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size,
                                uv_buf_t *buf) {

  RuntimeContext *ctx = (RuntimeContext *)handle->loop->data;
  ReadBuffer *buffer = ctx->http_ctx->acquire_read_buffer();
  if (buffer != nullptr) {
    buf->base = buffer->read_buffer;
    buf->len = 256;

  } else {
    buf->base = nullptr;
    buf->len = 0;
  }
}

// Move signal handlers to an a concern handled by the runtime itself
void Server::on_signal(uv_signal_t *handle, int signum) {
  uv_signal_stop(handle);
  uv_close((uv_handle_t *)handle, NULL);
  uv_stop(uv_default_loop());
}

void Server::on_peer_connected(uv_stream_t *server_stream, int status) {
  if (status < 0) {
    return;
  }
  int rc;

  RuntimeContext *env = (RuntimeContext *)server_stream->loop->data;
  ClientState *client = env->http_ctx->acquire_connection();
  if (client == nullptr) {
    std::cerr << "Connection pool is exhausted!\n";
    uv_tcp_t *temp_socket = env->http_ctx->acquire_emergency_handle();

    int rc = uv_tcp_init(uv_default_loop(),
                         temp_socket); // Needs to let libuv know about socket
    std::cout << rc << std::endl;
    if (uv_accept(server_stream, (uv_stream_t *)temp_socket) == 0) {
      uv_close((uv_handle_t *)temp_socket, on_client_closed_emergency);
    } else {
      env->http_ctx->release_emergency_handle(temp_socket);
    }

    return;
  }

  init_client_socket(client);

  if (uv_accept(server_stream, (uv_stream_t *)&client->socket) ==
      0) { // start listening to incoming requests
    struct sockaddr_storage peername;
    int namelen = sizeof(peername);
    if ((rc = uv_tcp_getpeername(&client->socket, (struct sockaddr *)&peername,
                                 &namelen)) < 0) {
      std::cout << "Get peer name failed " << uv_strerror(rc) << std::endl;
      return;
    }
    int r = uv_read_start((uv_stream_t *)&client->socket, on_alloc_buffer_cb,
                          on_read_cb);
  }
}

void Server::registerFuncHandler(const char *method, const char *uri,
                                 Handler handler) {
  this->_env->http_ctx->register_api_function(method, uri, handler);
}

Server::Server(int portNum, const char *portAddr)
    : portNum(portNum), portAddr(portAddr) {}

Server::Server(int portNum, RuntimeContext *env)
    : portNum(portNum), _env(env) {}

int Server::listen() { return 0; }
int Server::run() {
  int rc;
  struct sockaddr_in server_address;
  if ((rc = uv_ip4_addr("0.0.0.0", portNum, &server_address)) <
      0) { // No kernel interaction, simply fills a sockaddr_in struct in proc
           // memory
    cout << "uv_ip4_addr_init failed" << endl;
    return -1;
  }

  uv_signal_t sig;
  uv_loop_t *loop = this->_env->loop;
  uv_signal_init(loop, &sig);
  uv_signal_start(&sig, on_signal, SIGINT);

  //   ServerContext ctx;
  //   init_server_context(&ctx);

  uv_tcp_t server_stream;
  //   server_stream.data = &ctx;

  if ((rc = uv_tcp_init(loop, &server_stream)) <
      0) { // This is where the fd is created. Kernel has no idea what IP/Port
           // is belongs to
    cout << "uv_tcp_init_failed " << uv_strerror(rc) << endl;
    return -1;
  }

  if ((rc = uv_tcp_bind(&server_stream,
                        (const struct sockaddr *)&server_address, 0)) <
      0) { // The fd gets married to associate to address. So kernel knows
           // trarffic arriving at 0.0.0.0:9090 belongs to this fd
    cout << "uv_tcp_bind failed" << endl;
    return -1;
  }

  if ((rc = uv_listen((uv_stream_t *)&server_stream, N_BACKLOG,
                      on_peer_connected)) <
      0) { // Activates socket for incoming connections. Kernel starts doing TCP
           // handshakes and queueing them
    cout << "uv listen failed " << uv_strerror(rc) << endl;
  }

  cout << "Serving on port Test " << portNum << endl;
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  uv_loop_close(uv_default_loop());
  uv_library_shutdown();
  return 0;
}
