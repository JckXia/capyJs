# pragma once
#include "mem_pool.h"
#include <map>
#include <functional>
#include "client_state.h"


using Handler = std::function<void(RequestObject&, ResponseObject&)>;
struct HttpContext {
    MemPool<uv_tcp_t>* emergency_handles;
    MemPool<ClientState> *connection_pool;
    MemPool<ReadBuffer> *read_buffer_pool;
    std::map<std::pair<std::string, std::string>, Handler> routes_;
    ~HttpContext() {
        connection_pool->verify_no_leaks();
        read_buffer_pool->verify_no_leaks();
        delete connection_pool;
        delete read_buffer_pool;
    }

    void register_api_function(const char* method, const char * uri, Handler handler) {
        routes_[{method, uri}] = handler;
    }

    void invoke_function(RequestObject &req, ResponseObject& res,const char *method, const char* uri) {
        auto it = routes_.find({req.verb, req.uri});
        if (it != routes_.end()) {
            it->second(req,res);
        }
    }

    ClientState* acquire_connection() {
        return connection_pool->acquire();
    }

    void release_connection(ClientState *connection) {
        connection_pool->release(connection);
    }

    ReadBuffer* acquire_read_buffer() {
        return read_buffer_pool->acquire();
    }

    void release_read_buffer(ReadBuffer * buff) {
        read_buffer_pool->release(buff);
    }
};