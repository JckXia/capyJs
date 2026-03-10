#include <iostream>
#include "uv.h"
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

void on_client_closed(uv_handle_t* handle) {
    uv_tcp_t* client = (uv_tcp_t*)handle;
    free(client);
}

void on_peer_connected(uv_stream_t* server_stream, int status) {
    if (status < 0) {
        return;
    }
    int rc;
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(*client));  // Initialize an client structure on the heap
    if ((rc = uv_tcp_init(uv_default_loop(), client)) < 0) {    // create an fd mapped to the client for the kernel
        //die("uv_tcp_init failed: %s", uv_strerror(rc));
        return;
    }

    if(uv_accept(server_stream, (uv_stream_t*) client) == 0) {  // start listening to incoming requests
        struct sockaddr_storage peername;
        int namelen = sizeof(peername);
        if ((rc = uv_tcp_getpeername(client, (struct sockaddr*)&peername, &namelen)) <0){
            return;
        }   
        cout<<"Client connected!"<<endl;
        uv_close((uv_handle_t*)client, on_client_closed);
    }
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
 
    uv_tcp_t server_stream;
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
                        //("uv_listen failed: %s", uv_strerror(rc));
    }
    cout<<"Serving on port "<< portnum << endl;
    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return uv_loop_close(uv_default_loop());
}