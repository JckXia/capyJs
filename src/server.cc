#include "server.h"
#include "client_op.h"
#include <csignal>
#include "client_state.h"
#include "picohttpparser.h"
#include "quickjs.h"
#include "types.h"
#include "util.h"
#include "ws.h"
#include <map>
#include <string>
#include <vector>

Server::Server() {
  std::cout<<"init server!\n";
}

WSUpgradeInfo parseWSUpgrade(phr_header *headers, size_t num_headers) {
  WSUpgradeInfo info = {false, "", "", ""};

  bool hasUpgrade = false;
  bool hasConnection = false;

  for (size_t i = 0; i < num_headers; i++) {
    std::string name(headers[i].name, headers[i].name_len);
    std::string value(headers[i].value, headers[i].value_len);

    // Case-insensitive compare
    if (name.size() == 7 && strncasecmp(name.c_str(), "Upgrade", 7) == 0) {
      hasUpgrade = (strncasecmp(value.c_str(), "websocket", 9) == 0);
    } else if (name.size() == 10 &&
               strncasecmp(name.c_str(), "Connection", 10) == 0) {
      // Connection might be "Upgrade" or "keep-alive, Upgrade"
      hasConnection = (strcasestr(value.c_str(), "upgrade") != nullptr);
    } else if (name.size() == 17 &&
               strncasecmp(name.c_str(), "Sec-WebSocket-Key", 17) == 0) {
      info.key = value;
    } else if (name.size() == 21 &&
               strncasecmp(name.c_str(), "Sec-WebSocket-Version", 21) == 0) {
      info.version = value;
    } else if (name.size() == 22 &&
               strncasecmp(name.c_str(), "Sec-WebSocket-Protocol", 22) == 0) {
      info.protocol = value;
    }
  }

  info.isUpgrade = hasUpgrade && hasConnection && !info.key.empty();
  return info;
}
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

void Server::on_idle_timer_closed_cb(uv_handle_t *handle) {
  ClientState *client = (ClientState *)handle->data;
  client->server->pool.release_client(client);
}

void Server::on_idle_timeout_cb(uv_timer_t *handle) {
  ClientState *client = (ClientState *)handle->data;
  std::cout << "Idle timeout, closing connection\n";
  if (!uv_is_closing((uv_handle_t *)&client->socket)) {
    uv_close((uv_handle_t *)&client->socket, on_client_closed_cb);
  }
}

void Server::on_client_closed_cb(uv_handle_t *handle) {
  std::cout << "Client closing! \n";
  uv_tcp_t *client_sock = (uv_tcp_t *)handle;
  ClientState *client = (ClientState *)client_sock->data;
  client->closing = true;
  RuntimeContext *ctx = (RuntimeContext *)handle->loop->data;

  client_ops::clear_buffer(client);

  if (client->activeWs) {
    client->activeWs->onClose();
    JS_FreeValue(ctx->js_ctx, client->activeWs->sock_js);
    ctx->sock_alloc->release(client->activeWs);
    client->activeWs = nullptr;
  }

  uv_timer_stop(&client->idle_timer);
  uv_close((uv_handle_t *)&client->idle_timer, on_idle_timer_closed_cb);
  // ClientState is released in on_idle_timer_closed_cb once the timer handle is fully closed
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

void Server::process_http_1_request(uv_stream_t *client, ssize_t nread,
                                    const uv_buf_t *buf) {
  ClientState *client_state = (ClientState *)client->data;
  RuntimeContext *ctx = (RuntimeContext *)client->loop->data;
  ReadBuffer *rb = (ReadBuffer *)buf->base;
  rb->len = nread;

  client_ops::recv_new_buffer(client_state, rb);

  std::vector<char> buffer_data(client_state->recv_len);
  client_ops::flatten_buffer(client_state, buffer_data.data());

  const char *method;
  const char *path;
  int minor_version;
  struct phr_header headers[100];

  size_t method_len = 0;
  size_t path_len = 0;
  size_t num_headers = 100;

  ssize_t pret = phr_parse_request(buffer_data.data(), client_state->recv_len, &method,
                                   &method_len, &path, &path_len,
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
    client_ops::clear_buffer(client_state);
    uv_read_stop(client);
    return;
  }

  client_state->write_in_flight = true;

  RequestObject req;
  ResponseObject *res = new ResponseObject();

  size_t safe_verb_len = std::min(method_len, sizeof(req.verb) - 1);
  size_t safe_uri_len  = std::min(path_len,   sizeof(req.uri)  - 1);
  memcpy(req.verb, method, safe_verb_len);
  memcpy(req.uri,  path,   safe_uri_len);
  req.verb[safe_verb_len] = '\0';
  req.uri[safe_uri_len]   = '\0';

  std::cout << req.verb << " " << req.uri << "\n";

  client_ops::clear_buffer(client_state);

  WSUpgradeInfo info = parseWSUpgrade(headers, num_headers);
  if (info.isUpgrade) {
    // Switch protocol
    std::string accept_key = compute_ws_accept_key(info.key);
    WebSocket *ws = (WebSocket *)ctx->sock_alloc->alloc(sizeof(WebSocket),
                                                        AllocType::TYPE_WS);
    ws->cli = client;
    ctx->http_ctx->invoke_ws_function(*ws, req.uri); // The setup function
    ws->complete_protocol_upgrade_handshake(accept_key);
    delete res;
    return;
  }

  res->cli = client;
  ctx->http_ctx->invoke_function(
      req, *res, req.verb, req.uri); // TODO: Add enums like "INVOKE_SUCCESS",
                                     // "INVOKE_FAILED", "NOT_FOUND"
}

void Server::process_web_socket_request(uv_stream_t *client, ssize_t nread,
                                        const uv_buf_t *buf) {
  ClientState *client_state = (ClientState *)client->data;
  RuntimeContext *ctx = (RuntimeContext *)client->loop->data;
  WebSocket *ws = client_state->activeWs;

  ReadBuffer *rb = (ReadBuffer *)buf->base; // Need to release this
  rb->len = nread;
  client_ops::recv_new_buffer(client_state, rb);
  std::vector<char> buffer_data(client_state->recv_len);
  client_ops::flatten_buffer(client_state, buffer_data.data());
  int rc = ws->onMessage(buffer_data.data(), client_state->recv_len);

  if (rc == 0) {
    return;
  }

  client_ops::clear_buffer(client_state);
}
// Web Socket is initialized...client side
void Server::on_read_cb(uv_stream_t *client, ssize_t nread,
                        const uv_buf_t *buf) {
  ClientState *client_state = (ClientState *)client->data;
  RuntimeContext *ctx = (RuntimeContext *)client->loop->data;

  if (nread > 0) {
    uv_timer_again(&client_state->idle_timer);

    switch (client_state->conn_protocol) {
    case ConnectionProtocol::TYPE_HTTP_1:
      process_http_1_request(client, nread, buf);
      break;
    case ConnectionProtocol::TYPE_WEB_SOCKET:
      process_web_socket_request(client, nread, buf);
      break;
    default:
      assert(false && "protcol is not supported");
      break;
    }
  } else if (nread == UV_EOF) {

    client_state->server->pool.release_read_buffer((ReadBuffer *)buf->base);
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else if (nread == UV_ENOBUFS) {
    std::cout << "Throttling requests because pool ran out! \n";

    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else {

    client_state->server->pool.release_read_buffer((ReadBuffer *)buf->base);
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  }
}

void Server::on_alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size,
                                uv_buf_t *buf) {

  ClientState *client_state = (ClientState *)handle->data;
  ReadBuffer *buffer = client_state->server->pool.acquire_read_buffer();
  if (buffer != nullptr) {
    buf->base = buffer->read_buffer;
    buf->len = 256;

  } else {
    buf->base = nullptr;
    buf->len = 0;
  }
}

static void init_client_state(ClientState *cli) {
  cli->closing = false;
  cli->write_in_flight = false;
  cli->conn_protocol = ConnectionProtocol::TYPE_HTTP_1;
  cli->pending_write_buffer = nullptr;
  cli->recv_count = 0;
  cli->recv_len = 0;
  cli->recv_head = nullptr;
  cli->recv_tail = nullptr;
}

void Server::on_peer_connected(uv_stream_t *server_stream, int status) {
  if (status < 0) {
    return;
  }
  int rc;

  Server *server = (Server *)server_stream->data;
  RuntimeContext *env = server->_env;
  ClientState *client = server->pool.acquire_client();

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

  init_client_state(client);
  client->server = server;
  init_client_socket(client);
  uv_timer_init(env->loop, &client->idle_timer);
  client->idle_timer.data = client;
  uv_timer_start(&client->idle_timer, on_idle_timeout_cb, IDLE_TIMEOUT_MS, IDLE_TIMEOUT_MS);

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

void Server::registerWsFuncHandler(const char *uri, WSHandler handler) {
  this->_env->http_ctx->register_ws_cb(uri, handler);
}

Server::Server(int portNum, const char *portAddr)
    : portNum(portNum), portAddr(portAddr) {
    }

Server::Server(int portNum, RuntimeContext *env)
    : portNum(portNum), _env(env) {
    }

int Server::run() {
  _env->signal_ctx->register_daemon(_env->loop, SIGINT);

  int rc;
  if ((rc = uv_ip4_addr("0.0.0.0", portNum, &_server_addr)) <
      0) { // No kernel interaction, simply fills a sockaddr_in struct in proc
           // memory
    cout << "uv_ip4_addr_init failed" << endl;
    return -1;
  }

  uv_loop_t *loop = this->_env->loop;
  _server_stream.data = this;

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
