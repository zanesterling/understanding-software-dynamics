#pragma once

#include <stdint.h>

struct Connection {
  int sock_fd;
  uint32_t client_ip;
  uint32_t server_ip;
  uint16_t client_port;
  uint16_t server_port;
};

// Opens a TCP connection to the server with IP encoded in server_addr_str and
// port server_port, and stores the resulting connection in *out_conn.
//
// Returns 0 if successful, and -1 otherwise.
int tcp_connect(
  const char* server_addr_str,
  int server_port,
  Connection* out_conn
);

int tcp_listen(uint16_t server_port, int backlog);

int tcp_accept(int sockfd, Connection* connection);

