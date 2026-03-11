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
#define N_BACKLOG 10
#define MEM_POOL_SIZE 12000
const char* msg = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 25\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "hello from capyJs server\n";
// Keep an eye on this to ensure it doesnt grow too big
struct ClientState {
    uv_tcp_t socket;
    uv_write_t write_handle;
    
    MemPool<ClientState>* mem_pool; // self ref fror cleanup
    char read_buffer[256];
    bool write_in_flight = false;
};

struct ServerContext {
    MemPool<ClientState>* mem_pool;
};

void on_client_closed(uv_handle_t* handle) {
    uv_tcp_t* client_sock = (uv_tcp_t*)handle;
    ClientState* client = (ClientState*) client_sock->data;
    client->mem_pool->release(client); 
}

// After acquire() succeeded
void init_client_state(ClientState * client_state, MemPool<ClientState>* pool) {
    uv_tcp_t* client_sock = &client_state->socket;
    client_state->mem_pool = pool;
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

void on_write(uv_write_t* req, int status) {
    if(status) {
        std::cout<<"ERROR! "<< uv_strerror(status) << std::endl;
    } else {
         // std::cout<<"Write completed \n";
    }
    ClientState* client_state = (ClientState*) req->data;
    client_state->write_in_flight = false;
    // ClientState* client_state = (ClientState*) req->data;

    // auto client = &client_state->socket;
  //  uv_close((uv_handle_t*)&client_state->socket, on_client_closed); 
    // if (!uv_is_closing((uv_handle_t*)client)) {
    //     // Graceful shutdown: sends FIN to the client
    //     uv_shutdown_t* shutdown_req = (uv_shutdown_t*)malloc(sizeof(uv_shutdown_t));
    //     uv_shutdown(shutdown_req, (uv_stream_t*) client, [](uv_shutdown_t* req, int status) {
    //         // Once the shutdown (FIN) is acknowledged, we kill the handle
    //         uv_close((uv_handle_t*)req->handle, on_client_closed);
    //         free(req);
    //     });
    // }
}

void on_read(uv_stream_t* client, ssize_t nread, const uv_buf_t* buf) {
    if (nread > 0){
        // We got data!
        ClientState* client_state = (ClientState*)client->data;
       if(client_state->write_in_flight) {
            return;
       }
        client_state->write_in_flight = true;
        uv_buf_t buf = uv_buf_init((char*)msg, strlen(msg));
        uv_write_t* write_handle = &client_state->write_handle;
        write_handle->data = client_state;
        // uv_write(write_handle, (uv_stream_t*) &client_state->socket, &buf, 1, on_write);
        uv_write(write_handle, client, &buf, 1, on_write);
        return;
    }

    if (nread < 0){
        // nread < 0 means client closed the connection (UV_EOF)
        if(nread != UV_EOF) {
            
        }
        if (!uv_is_closing((uv_handle_t*)client)) {
            uv_close((uv_handle_t*)client, on_client_closed);
        }
    }
}

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t *buf) {
    ClientState* clientState = (ClientState*) handle->data;
    buf->base = clientState->read_buffer;
    buf->len = 256;
}


void on_peer_connected(uv_stream_t* server_stream, int status) {
    if (status < 0) {
        return;
    }
    int rc;
    ServerContext* ctx = (ServerContext*) server_stream->data;

    MemPool<ClientState>* pool = ctx->mem_pool;

    ClientState *client = pool->acquire();  //TODO: More graceful shutdown handling
    if(client == nullptr) {
        std::cerr<<"Connection pool exhausted!\n";
        // Pool exhausted — accept and immediately close to prevent backlog buildup
        // uv_tcp_t temp_socket;
        // uv_tcp_init(uv_default_loop(), &temp_socket);
        // if (uv_accept(server_stream, (uv_stream_t*) &temp_socket) == 0) {
         
        //     if (!uv_is_closing((uv_handle_t*)&temp_socket)) {
        //     uv_close((uv_handle_t*)&temp_socket, NULL);
        //   }
        // }
        return;
    }
    // ::cout<< pool->in_use_count() << std::endl;
    init_client_state(client, pool);
    init_client_socket(client);

    if(uv_accept(server_stream, (uv_stream_t*) &client->socket) == 0) {  // start listening to incoming requests
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);
        if ((rc = uv_tcp_getpeername(&client->socket, (struct sockaddr*)&peername, &namelen)) <0){
            std::cout<< "Get peer name failed "<< uv_strerror(rc) << std::endl;
            return;
        }   

   //     cout<<"Client HAS indeed connected!"<<endl;
        int r = uv_read_start((uv_stream_t*)&client->socket, alloc_buffer, on_read);
        // uv_buf_t buf = uv_buf_init((char*)msg, strlen(msg));
        // uv_write_t* write_handle = &client->write_handle;
        // write_handle->data = client;
        // uv_write(write_handle, (uv_stream_t*) &client->socket, &buf, 1, on_write);
       
    }
}

void on_signal(uv_signal_t* handle, int signum) {
    uv_signal_stop(handle);
    uv_close((uv_handle_t*)handle, NULL);
    uv_stop(uv_default_loop());
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
        cout <<"uv listen failed "<< uv_strerror(rc) << endl;
    }
    cout<<"Serving on port Test "<< portnum << endl;
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
 
    // memory_pool->dump_raw_state();
    memory_pool->verify_no_leaks();
    uv_loop_close(uv_default_loop());
    uv_library_shutdown();

    delete memory_pool;
    return 0;
}