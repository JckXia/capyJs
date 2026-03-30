#pragma once
#include "mapped_file.h"
#include <string>
#include <map>

struct FSContext {
    std::map<std::string, MappedFile> static_files;
};