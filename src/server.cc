#include "server.h"
#include "client_op.h"
#include "picohttpparser.h"
#include <map>
#include <string>

Server::Server() {}
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
  ctx->allocator->release((uv_tcp_t *)handle);
}

void Server::on_client_closed_cb(uv_handle_t *handle) {
  std::cout << "Client closing! \n";
  uv_tcp_t *client_sock = (uv_tcp_t *)handle;
  ClientState *client = (ClientState *)client_sock->data;
  client->closing = true;
  RuntimeContext *ctx = (RuntimeContext *)handle->loop->data;
  ctx->allocator->release(client);
}

void Server::on_write_cb(uv_write_t *req, int status) {
  if (status) {
    std::cout << "ERROR! " << uv_strerror(status) << std::endl;
  } else {
    // std::cout<<"Write completed \n";
  }

  ClientState *client_state = (ClientState *)req->data;
  RuntimeContext *ctx = (RuntimeContext *)req->handle->loop->data;
  client_state->write_in_flight = false; // dynamic writes
  ctx->allocator->release(client_state->pending_write_buffer);
  uv_read_start((uv_stream_t *)&client_state->socket, on_alloc_buffer_cb,
                on_read_cb);
}

void Server::on_static_write_cb(uv_write_t *req, int status) {
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

// builds a buffer for the headers and return its length
int build_headers_buf(char *header_buf, size_t header_len,
                      ResponseObject &res) {
  if (header_buf == nullptr || header_len == 0) {
    return -1;
  }

  if (res.header_size == 0) {
    int ret = snprintf(header_buf, header_len,
                       "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/plain\r\n"
                       "Content-Length: %zu\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n",
                       res.response_len);
    return ret;
  }

  size_t offset = 0;

  // Status line + standard headers (no terminating \r\n yet)
  offset += snprintf(header_buf, header_len,
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: keep-alive\r\n",
                     res.response_len);

  // Append custom headers
  for (auto &[header_key, header_value] : res.headers) {
    offset += snprintf(header_buf + offset, header_len - offset, "%s: %s\r\n",
                       header_key, header_value);
  }

  // NOW end the headers
  offset += snprintf(header_buf + offset, header_len - offset, "\r\n");
  return offset;
}

int build_headers(char *buf, size_t body_len, const char *content_type) {
  return snprintf(buf, 512,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: %s\r\n"
                  "Content-Length: %zu\r\n"
                  "Connection: keep-alive\r\n"
                  "\r\n",
                  content_type, body_len);
}
void Server::on_read_cb(uv_stream_t *client, ssize_t nread,
                        const uv_buf_t *buf) {
  ClientState *client_state = (ClientState *)client->data;
  RuntimeContext *ctx = (RuntimeContext *)client->loop->data;
                           

  if (nread > 0) {

    ReadBuffer *rb = (ReadBuffer *)buf->base;
    rb->len = nread;

    client_ops::recv_new_buffer(client_state, rb);

    char buffer_data[client_state->recv_len];
    client_ops::flatten_buffer(client_state, buffer_data);

    const char *method;
    const char *path;
    int minor_version;
    struct phr_header headers[100];

    size_t method_len = 0;
    size_t path_len = 0;
    size_t num_headers = 100;

    ssize_t pret = phr_parse_request(buffer_data, client_state->recv_len,
                                     &method, &method_len, &path, &path_len,
                                     &minor_version, headers, &num_headers, 0);

    // More packet might arrive!
    if (pret == -2) {
      return;
    }

    if (pret < 0) {
      if (!uv_is_closing((uv_handle_t *)client)) {
        uv_close((uv_handle_t *)client, on_client_closed_cb);
      }
      return;
    }

    if (client_state->write_in_flight || client_state->closing == true) {
      ctx->allocator->release(rb);
      uv_read_stop(client);
      return;
    }

    client_state->write_in_flight = true;

    RequestObject req;
    ResponseObject res;

    memcpy(req.verb, method, method_len);
    memcpy(req.uri, path, path_len);

    req.verb[method_len] = '\0';
    req.uri[path_len] = '\0';

    client_ops::clear_buffer(client_state, ctx);
    res.cli = client;
    ctx->http_ctx->invoke_function(
        req, res, req.verb, req.uri); // TODO: Add enums like "INVOKE_SUCCESS"
                                      // "INVOKE_FAILED" "ROUTE_NOT_FOUND"
 
  } else if (nread == UV_EOF) {

    ctx->allocator->release((ReadBuffer *)buf->base);
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else if (nread == UV_ENOBUFS) {
    std::cout << "Throttling requests because pool ran out! \n";

    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else {

    ctx->allocator->release((ReadBuffer *)buf->base);
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  }
}

void Server::on_alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size,
                                uv_buf_t *buf) {

  RuntimeContext *ctx = (RuntimeContext *)handle->loop->data;
  ReadBuffer *buffer = (ReadBuffer *)ctx->allocator->alloc(sizeof(ReadBuffer));
  if (buffer != nullptr) {
    buf->base = buffer->read_buffer;
    buf->len = 256;

  } else {
    buf->base = nullptr;
    buf->len = 0;
  }
}

void Server::on_peer_connected(uv_stream_t *server_stream, int status) {
  if (status < 0) {
    return;
  }
  int rc;

  RuntimeContext *env = (RuntimeContext *)server_stream->loop->data;
  ClientState *client =
      (ClientState *)env->allocator->alloc(sizeof(ClientState));

  if (client == nullptr) {
    std::cerr << "Connection pool is exhausted!\n";

    uv_tcp_t *temp_socket = (uv_tcp_t *)env->allocator->alloc(sizeof(uv_tcp_t));
    int rc = uv_tcp_init(uv_default_loop(),
                         temp_socket); // Needs to let libuv know about socket
    std::cout << rc << std::endl;
    if (uv_accept(server_stream, (uv_stream_t *)temp_socket) == 0) {
      uv_close((uv_handle_t *)temp_socket, on_client_closed_emergency);
    } else {
      env->allocator->release(temp_socket);
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

int Server::run() {
  int rc;
  struct sockaddr_in server_address;
  if ((rc = uv_ip4_addr("0.0.0.0", portNum, &_server_addr)) <
      0) { // No kernel interaction, simply fills a sockaddr_in struct in proc
           // memory
    cout << "uv_ip4_addr_init failed" << endl;
    return -1;
  }

  uv_loop_t *loop = this->_env->loop;
  uv_tcp_t server_stream;

  if ((rc = uv_tcp_init(loop, &_server_stream)) <
      0) { // This is where the fd is created. Kernel has no idea what IP/Port
           // is belongs to
    cout << "uv_tcp_init_failed " << uv_strerror(rc) << endl;
    return -1;
  }

  if ((rc = uv_tcp_bind(&_server_stream, (const struct sockaddr *)&_server_addr,
                        0)) <
      0) { // The fd gets married to associate to address. So kernel knows
           // trarffic arriving at 0.0.0.0:9090 belongs to this fd
    cout << "uv_tcp_bind failed" << endl;
    return -1;
  }

  if ((rc = uv_listen((uv_stream_t *)&_server_stream, N_BACKLOG,
                      on_peer_connected)) <
      0) { // Activates socket for incoming connections. Kernel starts doing TCP
           // handshakes and queueing them
    cout << "uv listen failed " << uv_strerror(rc) << endl;
  }

  return 0;
}
