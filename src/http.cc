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
/*
    What's the point of the N_BACKLOG?
        -> It's a traffic shaping mechanism to handle bursty traffic
        -> essentially leaky bucket as a queue technique! Except the drain rate is how fast the server can process the request
*/
#define N_BACKLOG 64
#define MEM_POOL_SIZE 10

// Keep an eye on this to ensure it doesnt grow too big
struct ClientState {
    uv_tcp_t socket;
    uv_write_t write_handle;
    
    MemPool<ClientState>* mem_pool; // self ref fror cleanup
};

struct ServerContext {
    MemPool<ClientState>* mem_pool;
};

void on_client_closed(uv_handle_t* handle) {
    uv_tcp_t* client_sock = (uv_tcp_t*)handle;
    ClientState* client = (ClientState*) client_sock->data;
    client->mem_pool->release(client); 
}

void on_peer_connected(uv_stream_t* server_stream, int status) {
    if (status < 0) {
        return;
    }
    int rc;
    ServerContext* ctx = (ServerContext*) server_stream->data;

    MemPool<ClientState>* pool = ctx->mem_pool;

    ClientState *client = pool->acquire();  //TODO: Abstract into initClient function
    uv_tcp_t* client_sock = &client->socket;
    client->mem_pool = pool;
    // uv_tcp_t* client = pool->acquire();
    client_sock->data = client;
    if ((rc = uv_tcp_init(uv_default_loop(), client_sock)) < 0) {    // create an fd mapped to the client for the kernel
        //die("uv_tcp_init failed: %s", uv_strerror(rc));
        return;
    }

    if(uv_accept(server_stream, (uv_stream_t*) client_sock) == 0) {  // start listening to incoming requests
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);
        if ((rc = uv_tcp_getpeername(client_sock, (struct sockaddr*)&peername, &namelen)) <0){
            return;
        }   
        cout<<"Client connected!"<<endl;
        
        uv_close((uv_handle_t*)client_sock, on_client_closed);
    }
}

void on_signal(uv_signal_t* handle, int signum) {
    uv_signal_stop(handle);
    uv_close((uv_handle_t*)handle, NULL);
    uv_stop(uv_default_loop());
}
 
int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    int portnum = 9090;
 
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
    MemPool<ClientState>* memory_pool = new MemPool<ClientState>(MEM_POOL_SIZE);  // Alloc'd on the HEAP bc this'd overflow in resource constrainted environments

    ctx.mem_pool = memory_pool;

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
        cout <<"uv listen failed"<< endl;
    }
    cout<<"Serving on port "<< portnum << endl;
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
 
    memory_pool->dump_raw_state();
    uv_loop_close(uv_default_loop());
    uv_library_shutdown();
    delete memory_pool;
    return 0;
}