#include<cstddef>
#include <map> // TODO: All of this needs to be repalced with deterministinc alloc
struct RequestObject {
  char verb[7];
  char uri[20];
};

struct ResponseObject {
  size_t response_len = 0;
  char response_buf[2048];
  std::map<const char *, const char *> headers;   
};
