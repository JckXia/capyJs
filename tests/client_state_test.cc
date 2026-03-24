#include <iostream>
#include "uv.h"
#include "server.h" //TODO: This is ugly..ClientState should be its own file...
#include "mem_pool.h"
#include <cassert>
void init_client_state(ClientState& cli) {
    cli.global_read_buffer = new MemPool<ReadBuffer>(200);
}

void tear_down_client_state(ClientState& cli) {
    cli.global_read_buffer->verify_no_leaks();
    delete cli.global_read_buffer;
}

void test_client_state_recv_new_buffer() {
    ClientState c;
    RequestObject o;
    init_client_state(c);
    uv_buf_t buf1 = uv_buf_init((char * )"abc", 3);
    uv_buf_t buf2 = uv_buf_init((char * )"def", 3);
    uv_buf_t buf3 = uv_buf_init((char * )"ghi", 3);

    c.recvNewBuffer(&buf1);
    c.recvNewBuffer(&buf2);
    c.recvNewBuffer(&buf3);
    assert(c.recv_count == 3);
    c.constructRequestObject(o);
 
    assert(strcmp(o.verb, "GET") == 0);
    tear_down_client_state(c);
}

void test_client_state_recv_new_buffer_arb_size() {
    ClientState c;
    RequestObject o;
    init_client_state(c);
    uv_buf_t buf1 = uv_buf_init((char * )"a", 1);
    uv_buf_t buf2 = uv_buf_init((char * )"def", 3);
    uv_buf_t buf3 = uv_buf_init((char * )"ghiz", 4);

    c.recvNewBuffer(&buf1);
    c.recvNewBuffer(&buf2);
    c.recvNewBuffer(&buf3);
    assert(c.recv_count == 3);
    c.constructRequestObject(o);
 
    assert(strcmp(o.verb, "GET") == 0);
    tear_down_client_state(c);
}



int main() {
    test_client_state_recv_new_buffer();
    test_client_state_recv_new_buffer_arb_size();
}