#pragma once

#include <stdint.h>

struct Connection {
  int sock_fd;
  uint32_t client_ip;
  uint32_t server_ip;
  uint16_t client_port;
  uint16_t server_port;
};

