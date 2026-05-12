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
    JSValue pending_cb;
    JSValue read_cb;
    JSContext *ctx = nullptr;
    FileSystem *owner = nullptr;

    FileEntry() { pending_cb = JS_UNDEFINED; }
};

struct FileSystem {
    std::map<std::string, FileEntry *> byPath;
    std::map<int, FileEntry *> byFd;

    ~FileSystem() {
        for (auto &[path, entry] : byPath) {
            delete[] entry->buf;
            delete entry;
        }
    }
};
