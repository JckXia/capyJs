#pragma once
#include "uv.h"
 

 
// TODO: Atm it will only work with a single file
struct FileSystem {
    int fd = -1;
    uv_fs_t* open_req;
    uv_fs_t* read_req;
    const char * filepath;
    JSValue open_cb;
    JSValue read_cb;
};