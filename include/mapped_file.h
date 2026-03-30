#pragma once

struct MappedFile {
    char* data;
    size_t size;
    int fd;
};