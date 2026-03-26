#include<cstddef>
struct RequestObject {
  char verb[7];
  char uri[20];
};

struct ResponseObject {
  const char *response;
  size_t response_len;
  char response_buf[2048]; 
};
