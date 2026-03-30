#pragma once

#include <string>
#include <map>
struct MappedFile {
    char* data;
    size_t size;
    int fd;
};

struct FSContext {
    std::map<std::string, MappedFile> static_files;
};