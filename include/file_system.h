#pragma once
#include "uv.h"
#include "quickjs.h"
#include <string>
#include <map>

struct FileSystem;

struct FileEntry {
    int fd = -1;
    char *buf = nullptr;
    size_t buf_len = 0;
    JSContext *ctx = nullptr;
    FileSystem *owner = nullptr;
};

// Idea:
//  -> FileOpState is fairly "static". It just tracks the fd across fs.* calls
//  -> It does not track the callbacks. Those are created/free'd on an per-call basis. See fs.cc
struct FileOpState {
    int fd = -1;
    char *buf = nullptr;
    size_t buf_len = 0;
   
    FileSystem *owner = nullptr;
    JSContext *ctx = nullptr;
 
 
    ~FileOpState() {
        delete[] this->buf;
    }
};

struct FileSystem {
    std::map<int, FileOpState*> fd_state; 
};
