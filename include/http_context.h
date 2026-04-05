#pragma once
#include "client_state.h"
#include "mem_pool.h"
#include <functional>
#include "quickjs.h"
#include "mapped_file.h"
#include <vector>
#include <map>

using Handler = std::function<void(RequestObject &, ResponseObject &)>;
struct HttpContext {
  std::vector<JSValue> registerd_cb; 
  MemPool<uv_tcp_t> *emergency_handles;
  MemPool<ClientState> *connection_pool;
  MemPool<ReadBuffer> *read_buffer_pool;
  std::map<std::string, MappedFile> static_files;
  std::map<std::pair<std::string, std::string>, Handler> routes_;

  void register_api_function(const char *method, const char *uri,
                             Handler handler) {
    routes_[{method, uri}] = handler;
  }

  void invoke_function(RequestObject &req, ResponseObject &res,
                       const char *method, const char *uri) {
    auto it = routes_.find({req.verb, req.uri});
    if (it != routes_.end()) {
      it->second(req, res);
    }
  }

  ClientState *acquire_connection() { 
    std::cout << "Acquiring connection object \n";
    return connection_pool->acquire(); 
  }

  void release_connection(ClientState *connection) {
    connection_pool->release(connection);
  }

  ReadBuffer *acquire_read_buffer() { return read_buffer_pool->acquire(); }

  void release_read_buffer(ReadBuffer *buff) {
    read_buffer_pool->release(buff);
  }

  uv_tcp_t *acquire_emergency_handle() { return emergency_handles->acquire(); }

  void release_emergency_handle(uv_tcp_t *handle) {
    emergency_handles->release(handle);
  }
};