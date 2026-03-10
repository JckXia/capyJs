#include <iostream>
#include "uv.h"
using namespace std;
/*
    Concept of an event loop:
        -> Loop is hiden inside libuv library
        -> user registers event handler and execute the loop
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
    uv_tcp_t* client = (uv_tcp_t*)malloc(sizeof(*client));
    if ((rc = uv_tcp_init(uv_default_loop(), client)) < 0) {
        //die("uv_tcp_init failed: %s", uv_strerror(rc));
        return;
    }

    if(uv_accept(server_stream, (uv_stream_t*) client) == 0) {
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
    cout<<"Serving on port "<< portnum << endl;
    
    int rc;
    uv_tcp_t server_stream;
    if ((rc = uv_tcp_init(uv_default_loop(), &server_stream)) < 0) {
        cout << "uv_tcp_init_failed "<< uv_strerror(rc) << endl;
        return -1;
    }

    struct sockaddr_in server_address;
    if ((rc = uv_ip4_addr("0.0.0.0", portnum, &server_address)) < 0) {
        cout<<"uv_ip4_addr_init failed" << endl;
        return -1;
    }

    if ((rc = uv_tcp_bind(&server_stream, (const struct sockaddr*)&server_address, 0)) <0) {
        cout<<"uv_tcp_bind failed" << endl;
        return -1;
    }

    if ((rc = uv_listen((uv_stream_t*)&server_stream, N_BACKLOG,  on_peer_connected)) < 0) {
        cout <<"uv listen failed"<< endl; 
                        //("uv_listen failed: %s", uv_strerror(rc));
    }

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);
    return uv_loop_close(uv_default_loop());
}