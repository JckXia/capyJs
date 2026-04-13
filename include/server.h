
#pragma once

#include "runtime_context.h"
#include "uv.h"

class RuntimeContext;
// Below can probably become variable params/config object
#define N_BACKLOG 10
#define MEM_POOL_SIZE 3
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
  explicit Server();
  explicit Server(int portNum, RuntimeContext *env);
  explicit Server(int portNum, const char *portAddr); // More options later
  void registerFuncHandler(const char *method, const char *uri,
                           Handler handler);

  int run();

  void setEnv(RuntimeContext *env) { this->_env = env; }

  void setPortNum(int portNum) { this->portNum = portNum; }

  // Libuv life cycles exposed to allow async context propagation/CB
  static void on_static_write_cb(uv_write_t *req, int status);
  static void on_write_cb(uv_write_t *req, int status);
  static void on_read_cb(uv_stream_t *client, ssize_t nread,
                         const uv_buf_t *buf);
  static void on_alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size,
                                 uv_buf_t *buf);
  static void on_client_closed_cb(uv_handle_t *handle);
  static void on_peer_connected(uv_stream_t *server_stream, int status);
  static void on_client_closed_emergency(uv_handle_t *handle);

  static void init_client_socket(ClientState *client_state);

private:
  static void process_http_1_request(uv_stream_t *client, ssize_t nread,
                                     const uv_buf_t *buf);
  static void process_web_socket_request(uv_stream_t *client, ssize_t nread,
                                         const uv_buf_t *buf);
  RuntimeContext *_env;
  struct sockaddr_in _server_addr;
  uv_tcp_t _server_stream;
  int portNum;
  const char *portAddr;
};
