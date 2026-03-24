
#pragma once

#include "mem_pool.h"
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
struct RequestObject {
  char verb[7];
  char uri[20];
};

struct ResponseObject {
  const char *response;
  size_t response_len;
};
using Handler = std::function<void(RequestObject&, ResponseObject&)>;

 

struct ReadBuffer {
  ReadBuffer(): next(nullptr) {}
  char read_buffer[256];
  size_t len;
  ReadBuffer *next;
};

struct ClientState {
  uv_tcp_t socket;
  uv_write_t write_handle;
  MemPool<ClientState> *mem_pool;
  MemPool<ReadBuffer> *global_read_buffer;
  bool write_in_flight = false;
  ReadBuffer *recv_head;
  ReadBuffer *recv_tail;
  size_t recv_count = 0;
  int recv_len = 0;
  ClientState() : recv_head(nullptr), recv_tail(nullptr) {}

  void recvNewBuffer(const uv_buf_t *buf) {
    ReadBuffer* buff = global_read_buffer->acquire();
    // TODO: Handle this
    if (buff != nullptr) {
        memcpy(buff->read_buffer, buf->base, buf->len);
        buff->len = buf->len;
        recv_len += buf->len;
        if(recv_head == nullptr) {
            recv_head = buff;
        } else{
            recv_tail->next = buff;
        }
        recv_count += 1;
        recv_tail = buff;
    }
  }
  
  void recvNewBuffer(ReadBuffer * buff) {
    if (buff != nullptr) {
        recv_len += buff->len;
        if (recv_head == nullptr) {
            recv_head = buff;
        } else {
            recv_tail->next = buff;
        }
        recv_count += 1;
        recv_tail = buff;
    }
   // ReadBuffer* buff = global_read_buffer->acquire();
    // TODO: Handle this
    // if (buff != nullptr) {
    //     // memcpy(buff->read_buffer, buf->base, buf->len);
    //     // buff->len = buf->len;
    //     recv_len += buf->len;
    //     if(recv_head == nullptr) {
    //         recv_head = buff;
    //     } else{
    //         recv_tail->next = buff;
    //     }
    //     recv_count += 1;
    //     recv_tail = buff;
    // }
  }

  void clearBuffer() {
    while (recv_head != nullptr) {
      ReadBuffer *currHead = recv_head;
      ReadBuffer *next = recv_head->next;
      global_read_buffer->release(currHead);
      recv_head = next;
      recv_count -= 1;
    }
    
    recv_count = 0;
    recv_len = 0;
    recv_head = nullptr;
    recv_tail = nullptr;
  }

  void serializeRecvBuffer() {
    ReadBuffer* wh = recv_head;
    char recv_buffer[recv_len + 1];
    size_t offset = 0;
   
    while (wh) {
        memcpy(recv_buffer + offset, wh->read_buffer, wh->len);
        offset += wh->len;
        wh = wh->next;
    }

    recv_buffer[recv_len] = '\0';
    std::cout<<recv_buffer<<std::endl;
  }

  // STUBBING http request
  void constructRequestObject(RequestObject& object) {
    // serializeRecvBuffer(); // More of an helper function for debugging
    memcpy(object.verb, "GET", 3);
    memcpy(object.uri, "/hello",6);
    object.verb[3] = '\0';
    object.uri[6] = '\0';
  }
};
// We might move to an approach that registers URL mapping against function
// handler (i.e express' app.get("/get", func(){}) style) This should be enough
// for embedding with V8 for now. It encapsualtes a lot of the libuv plumbing
// under the hood Actually, let's just do it right and get it over with
class Server {
public:
  explicit Server(int portNum, const char *portAddr); // More options later
  void registerFuncHandler(const char * method, const char * uri, Handler handler);
 
  int run();

private:
  int portNum;
  const char *portAddr;
  std::map<std::pair<std::string, std::string>, Handler> routes_;
  // Helper structs:
  struct ServerContext {
    std::map<std::pair<std::string, std::string>, Handler> routes_;
    MemPool<uv_tcp_t> *emergency_handles;
    MemPool<ClientState> *mem_pool;
    MemPool<ReadBuffer> *read_buffer_pool;
    ~ServerContext() {
      mem_pool->verify_no_leaks();
      read_buffer_pool->verify_no_leaks();
      delete mem_pool;
      delete read_buffer_pool;
    }
  };
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
  // General helper functions
  void init_server_context(ServerContext *ctx);
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
  uv_tcp_t *client_sock = (uv_tcp_t *)handle;
  ServerContext *ctx = (ServerContext *)client_sock->data;
  ctx->emergency_handles->release(client_sock);
}

void Server::on_client_closed_cb(uv_handle_t *handle) {
  uv_tcp_t *client_sock = (uv_tcp_t *)handle;
  ClientState *client = (ClientState *)client_sock->data;
  client->mem_pool->release(client);
}
void Server::on_write_cb(uv_write_t *req, int status) {
  if (status) {
    std::cout << "ERROR! " << uv_strerror(status) << std::endl;
  } else {
    // std::cout<<"Write completed \n";
  }
  ClientState *client_state = (ClientState *)req->data;
  client_state->write_in_flight = false;
}

void Server::on_read_cb(uv_stream_t *client, ssize_t nread,
                        const uv_buf_t *buf) {
  ClientState *client_state = (ClientState *)client->data;
 
  if (nread > 0) {
 
    ReadBuffer* rb = (ReadBuffer*) buf->base;
    rb->len = nread;
 
 
    // client_state->global_read_buffer->release((ReadBuffer *)buf->base);
    if (client_state->write_in_flight) {
      client_state->global_read_buffer->release(rb);
      return;
    }
    client_state->write_in_flight = true;
    client_state->recvNewBuffer(rb);
    
    RequestObject req;
    ResponseObject res;
    client_state->constructRequestObject(req);
    client_state->clearBuffer();
    ServerContext * ctx =  (ServerContext*) uv_default_loop()->data;
    auto routes = ctx->routes_;
    auto it = routes.find({req.verb, req.uri});
    if (it != routes.end()) {
        it->second(req, res);
    } else {
        // TODO: Add handling for route-not-found, or throw an exception
    }

    uv_buf_t buff = uv_buf_init((char *)res.response, res.response_len);
    uv_write_t *write_handle = &client_state->write_handle;
    write_handle->data = client_state;
    int rc;
    if ((rc = uv_write(write_handle, client, &buff, 1, on_write_cb)) < 0) {
      std::cout << "Write to socket failed! " << uv_strerror(rc) << std::endl;
    }
  } else if (nread == UV_EOF) {
    // client_state->clearBuffer();
    client_state->global_read_buffer->release((ReadBuffer *)buf->base);     
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else if (nread == UV_ENOBUFS) {
    std::cout<< "Throttling requests because pool ran out! \n";
    //client_state->clearBuffer();
    //client_state->global_read_buffer->release((ReadBuffer *)buf->base);
    
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  } else {
    // client_state->clearBuffer();
    client_state->global_read_buffer->release((ReadBuffer *)buf->base);
    
    // Clsoe 
    if (!uv_is_closing((uv_handle_t *)client)) {
      uv_close((uv_handle_t *)client, on_client_closed_cb);
    }
  }
}

void Server::on_alloc_buffer_cb(uv_handle_t *handle, size_t suggested_size,
                                uv_buf_t *buf) {

  ClientState *clientState = (ClientState *)handle->data;
  ReadBuffer *buffer = clientState->global_read_buffer->acquire();
  if (buffer != nullptr) {
    buf->base = buffer->read_buffer;
    buf->len = 256;
 
  } else {
    buf->base = nullptr;
    buf->len =0;
 
  }
}

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
  ServerContext *ctx = (ServerContext *)server_stream->data;

  MemPool<ClientState> *pool = ctx->mem_pool;
  MemPool<ReadBuffer> *read_buffer = ctx->read_buffer_pool;

  ClientState *client = pool->acquire();
  if (client == nullptr) {
    std::cerr << "Connection pool is exhausted!\n";
    uv_tcp_t *temp_socket = ctx->emergency_handles->acquire();
    temp_socket->data = ctx;
    int rc = uv_tcp_init(uv_default_loop(),
                         temp_socket); // Needs to let libuv know about socket
    std::cout << rc << std::endl;
    if (uv_accept(server_stream, (uv_stream_t *)temp_socket) == 0) {
      uv_close((uv_handle_t *)temp_socket, on_client_closed_emergency);
    } else {
      ctx->emergency_handles->release(temp_socket);
    }

    return;
  }

  client->mem_pool = pool;
  client->global_read_buffer = read_buffer;
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

void Server::init_server_context(ServerContext *ctx) {
  MemPool<ClientState> *memory_pool = new MemPool<ClientState>(
      MEM_POOL_SIZE); // Alloc'd on the HEAP bc this'd overflow in resource
                      // constrainted environments
  MemPool<ReadBuffer> *read_buffer =
      new MemPool<ReadBuffer>(READ_BUFFER_POOL_SIZE); //
  MemPool<uv_tcp_t> emergency_handles(16);
  ctx->emergency_handles = &emergency_handles;
  ctx->mem_pool = memory_pool;
  ctx->read_buffer_pool = read_buffer;
  ctx->routes_ = routes_;
}

void Server::registerFuncHandler(const char * method, const char* uri, Handler handler) {
    routes_[{method, uri}] = handler;
}

Server::Server(int portNum, const char *portAddr)
    : portNum(portNum), portAddr(portAddr) {}

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
  uv_signal_init(uv_default_loop(), &sig);
  uv_signal_start(&sig, on_signal, SIGINT);

  ServerContext ctx;
  init_server_context(&ctx);

  uv_tcp_t server_stream;
  server_stream.data = &ctx;
  
  if ((rc = uv_tcp_init(uv_default_loop(), &server_stream)) <
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

  uv_loop_t* loop = uv_default_loop();
  loop->data = &ctx;
  
  cout << "Serving on port Test " << portNum << endl;
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  uv_loop_close(uv_default_loop());
  uv_library_shutdown();
  return 0;
}
