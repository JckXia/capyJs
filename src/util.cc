#include "util.h"
#include <cstdio>
#include <cstddef>
#include <cstdlib>
char *read_file(const char *filename, size_t *out_len) {
  FILE *f = fopen(filename, "rb");
  if (!f) {
    fprintf(stderr, "Error: cannot open '%s'\n", filename);
    return nullptr;
  }

  fseek(f, 0, SEEK_END);
  size_t len = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buf = (char *)malloc(len + 1);
  if (!buf) {
    fclose(f);
    return nullptr;
  }

  fread(buf, 1, len, f);
  buf[len] = '\0';
  fclose(f);

  if (out_len)
    *out_len = len;
  return buf;
}