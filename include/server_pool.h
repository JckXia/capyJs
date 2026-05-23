#pragma once
#include "mem_pool.h"
#include "conn_guard.h"
#include "client_state.h"

// Default sizing — tunable at Server construction time.
// ReadBuffer headroom is 8x connections: partial HTTP reads and WebSocket
// frames can queue several buffers per connection before they're drained.
static constexpr size_t DEFAULT_MAX_CONNECTIONS = 512;
static constexpr size_t DEFAULT_READ_BUFFER_HEADROOM = 8;

struct ServerPool {
    MemPool<ClientState> clients;
    MemPool<ReadBuffer>  read_buffers;
    MemPool<ConnGuard>   guards;

    explicit ServerPool(size_t max_connections = DEFAULT_MAX_CONNECTIONS)
        : clients(max_connections, "ClientPool")
        , read_buffers(max_connections * DEFAULT_READ_BUFFER_HEADROOM,
                       "ReadBufferPool")
        , guards(max_connections, "ConnGuardPool") {}

    ClientState  *acquire_client()                   { return clients.acquire(); }
    void          release_client(ClientState *c)     { clients.release(c); }
    ReadBuffer   *acquire_read_buffer()              { return read_buffers.acquire(); }
    void          release_read_buffer(ReadBuffer *rb){ read_buffers.release(rb); }
    ConnGuard    *acquire_guard()                    { return guards.acquire(); }

    void verify_no_leaks() const {
        clients.verify_no_leaks();
        read_buffers.verify_no_leaks();
        guards.verify_no_leaks();
    }
};
