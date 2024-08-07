#include "print_hex.h"

void print_hex(const void* const buf, const size_t n_bytes) {
  for (size_t i = 0; i < n_bytes; ++i) {
    printf(" %02x", ((uint8_t*)buf)[i]);
  }
  putc('\n', stdout);
}

