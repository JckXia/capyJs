#include <iostream>
#include "uv.h"
#include "mem_pool.h"
using namespace std;
/*
    Libuv's uv_run:
        while (there are pending handles or requests) {
            poll_for_io()
            run_ready_callbacks
        }
    No threads are created for these callbacks. Uses OS's I/O notification systems to know 
    when something is ready and then execute custom registered callback on the single main thread

    Note, there's a single fd that maps to the server during the life time of the server.

*/
#define N_BACKLOG 10
#define MEM_POOL_SIZE 10000
#define READ_BUFFER_POOL_SIZE 200
const char* msg = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 25\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "hello from capyJs server\n";
struct ReadBuffer {
    char read_buffer[256];
};
// Keep an eye on this to ensure it doesnt grow too big
struct ClientState {
    uv_tcp_t socket;
    uv_write_t write_handle;
    
    MemPool<ClientState>* mem_pool; // self ref fror cleanup
    MemPool<ReadBuffer>* global_read_buffer;
    bool write_in_flight = false;
};


// This struct manages all resources, caches used by the server process
struct ServerContext {
 
    MemPool<uv_tcp_t>* emergency_handles;
    MemPool<ClientState>* mem_pool;
    MemPool<ReadBuffer>* read_buffer_pool;
    ~ServerContext() {
        mem_pool->verify_no_leaks(); 
        read_buffer_pool->verify_no_leaks();
        delete mem_pool;
        delete read_buffer_pool;
    }
};


void on_client_closed_emergency(uv_handle_t* handle) {
    uv_tcp_t* client_sock = (uv_tcp_t*)handle;
    ServerContext * ctx = (ServerContext*)client_sock->data;
    ctx->emergency_handles->release(client_sock);
}

void on_client_closed(uv_handle_t* handle) {
    uv_tcp_t* client_sock = (uv_tcp_t*)handle;
    ClientState* client = (ClientState*) client_sock->data;
    client->mem_pool->release(client); 
}

void init_client_socket(ClientState* client_state) {
    uv_tcp_t* client_sock = &client_state->socket;
    client_sock->data = client_state;
    int rc;
    if ((rc = uv_tcp_init(uv_default_loop(), client_sock)) < 0) {    // create an fd mapped to the client for the kernel
        //die("uv_tcp_init failed: %s", uv_strerror(rc));
        std::cout<<" UV TCP INIT FAILED " << uv_strerror(rc) << std::endl;
        return;
    }
}
// ########################################################### Above are helper functions ###################### //


// ######################################################## Libuv life cycle callbacks ######## //
void on_write(uv_write_t* req, int status) {
    if(status) {
        std::cout<<"ERROR! "<< uv_strerror(status) << std::endl;
    } else {
         // std::cout<<"Write completed \n";
    }
    ClientState* client_state = (ClientState*) req->data;
    client_state->write_in_flight = false;
 
}

void print_read_request(const uv_buf_t* buf) {
    auto base = buf->base;
    int len= buf->len;
    for (int i =0;i< len; i ++) {
        cout<<base[i];
    }
    cout<<endl;
}


void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    ClientState* client_state = (ClientState*)client->data;
    client_state->global_read_buffer->release((ReadBuffer*) buf->base);
    if (nread > 0){
        // We got data!
  
       if(client_state->write_in_flight) {
            return;
       }
      // print_read_request(buf);
        // client_state->global_read_buffer->release((ReadBuffer*) buf->base);
        client_state->write_in_flight = true;
        uv_buf_t buff = uv_buf_init((char*)msg, strlen(msg));
        uv_write_t* write_handle = &client_state->write_handle;
        write_handle->data = client_state;
        int rc;
        if (rc = uv_write(write_handle, client, &buff, 1, on_write) < 0) {
            std::cout<<"Write to socket failed! " << uv_strerror(rc) << std::endl;
        }
        return;
    }

    if (nread < 0){
        // client_state->global_read_buffer->release((ReadBuffer*) buf->base);
        // nread < 0 means client closed the connection (UV_EOF)
        if(nread != UV_EOF) {
            
        }
        if (!uv_is_closing((uv_handle_t*)client)) {
            uv_close((uv_handle_t*)client, on_client_closed);
        }
    }
}

 
// Assume we are only getting a SINGLE chunk at the moemnt..ignoring suggested_size
void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t *buf) {
    ClientState* clientState = (ClientState*) handle->data;
    
    ReadBuffer* buffer = clientState->global_read_buffer->acquire();
    if(buffer != nullptr) {
        buf->base = buffer->read_buffer;
        buf->len = 256;
    }
 
}


void on_peer_connected(uv_stream_t* server_stream, int status) {
    if (status < 0) {
        return;
    }
    int rc;
    ServerContext* ctx = (ServerContext*) server_stream->data;

    MemPool<ClientState>* pool = ctx->mem_pool;
    MemPool<ReadBuffer>* read_buffer = ctx->read_buffer_pool;

    ClientState *client = pool->acquire();
    if(client == nullptr) {
        std::cerr<<"Connection pool is exhausted!\n";
        uv_tcp_t* temp_socket = ctx->emergency_handles->acquire();
        temp_socket->data = ctx;
        int rc = uv_tcp_init(uv_default_loop(), temp_socket); // Needs to let libuv know about socket
        std::cout<< rc << std::endl;
        if(uv_accept(server_stream, (uv_stream_t*) temp_socket) == 0) {
            uv_close((uv_handle_t*)temp_socket, on_client_closed_emergency);
        } else {
            ctx->emergency_handles->release(temp_socket);
        }
 
        return;
    }

    client->mem_pool = pool;
    client->global_read_buffer = read_buffer;
    init_client_socket(client);

    if(uv_accept(server_stream, (uv_stream_t*) &client->socket) == 0) {  // start listening to incoming requests
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);
        if ((rc = uv_tcp_getpeername(&client->socket, (struct sockaddr*)&peername, &namelen)) <0){
            std::cout<< "Get peer name failed "<< uv_strerror(rc) << std::endl;
            return;
        }   
        int r = uv_read_start((uv_stream_t*)&client->socket, alloc_buffer, on_read);
    }
}

void on_signal(uv_signal_t* handle, int signum) {
    uv_signal_stop(handle);
    uv_close((uv_handle_t*)handle, NULL);
    uv_stop(uv_default_loop());
}

void init_server_context(ServerContext * ctx) {
    MemPool<ClientState>* memory_pool = new MemPool<ClientState>(MEM_POOL_SIZE);  // Alloc'd on the HEAP bc this'd overflow in resource constrainted environments
    MemPool<ReadBuffer>* read_buffer = new MemPool<ReadBuffer>(READ_BUFFER_POOL_SIZE); // 
    MemPool<uv_tcp_t> emergency_handles(16);
    ctx->emergency_handles = &emergency_handles;
    ctx->mem_pool = memory_pool;
    ctx->read_buffer_pool = read_buffer;
}
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    int portnum = 9091;
 
    int rc;
    struct sockaddr_in server_address;
    if ((rc = uv_ip4_addr("0.0.0.0", portnum, &server_address)) < 0) { // No kernel interaction, simply fills a sockaddr_in struct in proc memory
        cout<<"uv_ip4_addr_init failed" << endl;
        return -1;
    }
    
    uv_signal_t sig;
    uv_signal_init(uv_default_loop(), &sig);
    uv_signal_start(&sig, on_signal, SIGINT);

    ServerContext ctx;
    init_server_context(&ctx);
 
    uv_tcp_t server_stream;
    server_stream.data = &ctx;

    if ((rc = uv_tcp_init(uv_default_loop(), &server_stream)) < 0) {  // This is where the fd is created. Kernel has no idea what IP/Port is belongs to
        cout << "uv_tcp_init_failed "<< uv_strerror(rc) << endl;
        return -1;
    }

    if ((rc = uv_tcp_bind(&server_stream, (const struct sockaddr*)&server_address, 0)) <0) { // The fd gets married to associate to address. So kernel knows trarffic arriving at 0.0.0.0:9090 belongs to this fd
        cout<<"uv_tcp_bind failed" << endl;
        return -1;
    }

    if ((rc = uv_listen((uv_stream_t*)&server_stream, N_BACKLOG,  on_peer_connected)) < 0) { // Activates socket for incoming connections. Kernel starts doing TCP handshakes and queueing them
        cout <<"uv listen failed "<< uv_strerror(rc) << endl;
    }
    cout<<"Serving on port Test "<< portnum << endl;
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
 
 
    uv_loop_close(uv_default_loop());
    uv_library_shutdown();
    return 0;
}