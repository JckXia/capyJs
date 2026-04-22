#include "util.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
void sha1(const uint8_t *data, size_t len, uint8_t *digest) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

  // Pad message: original + 0x80 + zeros + 64-bit length
  size_t padded_len = ((len + 8) / 64 + 1) * 64;
  uint8_t *msg = new uint8_t[padded_len]();
  memcpy(msg, data, len);
  msg[len] = 0x80;
  uint64_t bit_len = len * 8;
  for (int i = 0; i < 8; i++) {
    msg[padded_len - 1 - i] = (bit_len >> (i * 8)) & 0xFF;
  }

  // Process 64-byte blocks
  for (size_t offset = 0; offset < padded_len; offset += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
      w[i] = (msg[offset + i * 4] << 24) | (msg[offset + i * 4 + 1] << 16) |
             (msg[offset + i * 4 + 2] << 8) | msg[offset + i * 4 + 3];
    }
    for (int i = 16; i < 80; i++) {
      uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
      w[i] = (x << 1) | (x >> 31);
    }

    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      uint32_t f, k;
      if (i < 20) {
        f = (b & c) | (~b & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }

      uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
      e = d;
      d = c;
      c = (b << 30) | (b >> 2);
      b = a;
      a = temp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
  }

  delete[] msg;

  for (int i = 0; i < 5; i++) {
    digest[i * 4] = (h[i] >> 24) & 0xFF;
    digest[i * 4 + 1] = (h[i] >> 16) & 0xFF;
    digest[i * 4 + 2] = (h[i] >> 8) & 0xFF;
    digest[i * 4 + 3] = h[i] & 0xFF;
  }
}

std::string base64(const uint8_t *d, size_t len) {
  static const char *t =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = d[i] << 16 | (i + 1 < len ? d[i + 1] << 8 : 0) |
                 (i + 2 < len ? d[i + 2] : 0);
    o += t[(n >> 18) & 0x3F];
    o += t[(n >> 12) & 0x3F];
    o += (i + 1 < len) ? t[(n >> 6) & 0x3F] : '=';
    o += (i + 2 < len) ? t[n & 0x3F] : '=';
  }
  return o;
}

std::string compute_ws_accept_key(const std::string &incoming_sec_key) {
  const std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  std::string combined = incoming_sec_key + magic;
  uint8_t digest[20];
  sha1((const uint8_t *)combined.data(), combined.size(), digest);
  return base64(digest, 20);
}