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

- # 3/15/2026:
#### Completed:
- Merged pull requests and pulled in libuv as a dep nodeJs style

# 3/16/2026:
- Implement a separate read/write buffer pool for the server component (Done)

#  3/23/2026:
- Abstract out the server into an agnostic interface class for future embedding (Done)
- Embed server with V8 (In Progress)


# Overarching goals:
- Server capable of withstanding C10K barrage (Done!)
- Can register URI routing with server + function callbacks (Done!)
- Embed QuickJs (In Progress)
- Parsing HTTP requests using llhttp (Todo)
- NOT have the function callback be responsible for "decorating" the response object into a correct payload (headers, etc) (Todo)
- Server capable of serving static files (like html, css, etc) to caller (Todo)
- Support Stream, Timer, FileSystem and Fetch (Todo)
