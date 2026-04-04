#include <iostream>
#include "uv.h"
#include "client_op.h"
 
#include <cassert>
#include <string.h>
 
void test_creation_set_head_tail_null() {
    ClientState c;
    assert(c.recv_head == nullptr);
    assert(c.recv_tail == nullptr);
}

void test_client_recv_buffer() {
    ClientState c;
    ReadBuffer buf1;
    buf1.read_buffer[0] = 'a';
    buf1.len = 1;

    ReadBuffer buf2;
    buf2.read_buffer[0] = 'b';
    buf2.len = 1;
    ReadBuffer buf3;
    buf3.read_buffer[0] = 'c';
    buf3.len = 1;

    client_ops::recv_new_buffer(&c, &buf1);
    client_ops::recv_new_buffer(&c, &buf2);
    client_ops::recv_new_buffer(&c, &buf3);
    assert(client_ops::get_recv_packet_count(&c) == 3);
    assert(client_ops::get_recv_packet_count(&c) == 3);
}

void test_client_recv_and_clear_buffer() {
    HttpContext http_ctx;
    MemPool<uv_tcp_t> ehandle(16);
    http_ctx.emergency_handles = &ehandle;
    http_ctx.connection_pool = new MemPool<ClientState> (16);
    http_ctx.read_buffer_pool = new MemPool<ReadBuffer> (16);

    RuntimeContext ctx;
    ctx.http_ctx = &http_ctx;

    ClientState c;
    ReadBuffer* buf1 = ctx.http_ctx->acquire_read_buffer();
    strcpy(buf1->read_buffer, "abc");  
    buf1->len = 3;

    ReadBuffer* buf2 = ctx.http_ctx->acquire_read_buffer();
    strcpy(buf2->read_buffer, "jkl");  
    buf2->len = 3;
    
    client_ops::recv_new_buffer(&c, buf1);
    client_ops::recv_new_buffer(&c, buf2);
    assert(client_ops::get_recv_packet_count(&c) == 2);
    client_ops::print_recv_buffer(&c);
    client_ops::clear_buffer(&c,&ctx);
}

void test_client_populate_req() {
    ClientState c;
    RequestObject req;
    client_ops::populate_request_object(&c, req);
    assert(strcmp(req.verb, "GET") == 0);
}



int main() {
   test_client_recv_buffer();
   test_client_recv_and_clear_buffer();
   test_client_populate_req();
}