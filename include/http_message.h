#include<cstddef>
struct RequestObject {
  char verb[7];
  char uri[20];
};

struct ResponseObject {
  size_t response_len = 0;
  char response_buf[2048]; 
};
