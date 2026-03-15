# 3/11/2026:
#### Completed:
- Working "hello world" server
- Graceful shutdown
- Verified with Valgrind to see that there's no memory leaks beyond initial allocs

#### Next steps:
- Each client has an 64 byte buffer. This is not gonna scale
- Instead, we'll allocate a separate buffer pool. (WARNING: watchout for the casts. They'll bite you)
- Each ClientState should track: StartOfReadBuffer, nLen. (Since the buffers are allocated sequentially)
- Need to also refactor out the "data" field. It's a mess at the moment

# 3/12/2026:
#### Completed:
- Improved error handling around when connection MemPool is exhausted

# 3/14/2026:
#### Completed:
- Rewrote the memory pool a bit. removed prev pointers. Tests passed!

# 3/16/2026 (LC week goals):
- Implement a separate buffer pool for the server
- Look into and implement streaming static files back to caller, handle back pressure

#  3/23/2026 (Runtime week goals):
- Integrate llhttp into server.
- Abstract out the server into an agnostic interface class for future embedding
- Design an extensive load test for the server and the memory pool. Export results into diagrams.


