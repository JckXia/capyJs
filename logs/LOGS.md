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