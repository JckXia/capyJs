#pragma once
#include "uv.h"
#include "quickjs.h"
#include <string>
#include <map>

struct FileSystem;

enum FileState {
    UNLINKED,
    PENDING,
    OPEN,
    READ,
    CLOSE
};

struct FileEntry {
    int fd = -1;
    char *buf = nullptr;
    size_t buf_len = 0;
    JSValue pending_cb;
    JSValue read_cb;
    JSContext *ctx = nullptr;
    FileSystem *owner = nullptr;

    FileEntry() { pending_cb = JS_UNDEFINED; }
};

// Idea:
//  -> Each FileOpState corresponds to a single "fd"'s life cycle.
//  -> File open operation against the same file path, usnig the same callback, are considered distinct because 
//     underneath, a new fd is acquired from the OS.
//  -> The life cycle is linear like this:
//      -> fs.open(). We persist the provided open JS callback, to be invoked when libuv retrieves an fd successfully
//          -> When this is done, this JS function can be free'd. Update state
//      -> fs.read(fd), we start reading from the fd, from the start, providing buffer length. Again, persist the read callback to FileOp
//          -> When libuv read callback is invoked, we free this read_callback JS function. Update state
//              -> TODO: This is assuming we are processing stuff in one shot. Does not support file stream yet.
//      -> fs.close(fd):
//          -> When libuv "close" callback is invoked, os has reclaimed the fd, and we can cleanup this data structure
struct FileOpState {
    int fd = -1;
    char *buf = nullptr;
    size_t buf_len = 0;
    FileState file_state;
    FileSystem *owner = nullptr;
    JSContext *ctx = nullptr;
    JSValue callback;
    JSValue read_callback;
    FileOpState() {
        file_state = FileState::UNLINKED;
        callback = JS_UNDEFINED;
        read_callback = JS_UNDEFINED;
    }
    ~FileOpState() {
        delete[] this->buf;
    }
};

struct FileSystem {
    // std::map<std::string, FileEntry *> byPath;
    // std::map<int, FileEntry *> byFd;
    std::map<int, FileOpState*> fd_state; 

    // ~FileSystem() {
    //     for (auto &[path, entry] : byPath) {
    //         delete[] entry->buf;
    //         delete entry;
    //     }
    // }
};
