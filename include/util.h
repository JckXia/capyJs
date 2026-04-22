#pragma once
#include <cstddef>
#include <string>
char *read_file(const char *filename, size_t *out_len);
std::string compute_ws_accept_key(const std::string &incoming_sec_key);
const char *get_file_extension(const char *filename);
const char *get_content_type(const char *ext);