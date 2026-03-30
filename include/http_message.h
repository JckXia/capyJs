#include<cstddef>
#include <map> // TODO: All of this needs to be repalced with deterministinc alloc
struct RequestObject {
  char verb[7];
  char uri[20];
};

// TODO: Refactor this
struct ResponseObject {
  size_t response_len = 0;
  char response_buf[8192];
  std::map<const char *, const char *> headers;   
  bool is_static = false;
  char *static_data;
};
