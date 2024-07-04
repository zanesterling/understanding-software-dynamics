#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <strings.h>

int tcp_connect(
  const char* const server_addr_str,
  const int server_port,
  Connection* out_conn
) {
  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == sock_fd) return -1;

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  if (-1 == inet_pton(AF_INET, server_addr_str, &server_addr.sin_addr)) {
    int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }

  if (-1 == connect(sock_fd, (sockaddr*)&server_addr, sizeof(server_addr))) {
    int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }

  sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  if (-1 == getsockname(sock_fd, (sockaddr*)&client_addr, &addr_len)) {
    int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }
  if (addr_len != sizeof(client_addr)) {
    errno = ENOANO;
    return -1;
  }

  out_conn->sock_fd = sock_fd;
  out_conn->server_ip   = server_addr.sin_addr.s_addr;
  out_conn->server_port = server_addr.sin_port;
  out_conn->client_ip   = client_addr.sin_addr.s_addr;
  out_conn->client_port = client_addr.sin_port;
  return 0;
}

int tcp_listen(const uint16_t port, const int backlog) {
  // Open socket.
  int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (-1 == sock_fd) return -1;

  // Bind socket.
  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bzero(server_addr.sin_zero, 8);
  if (-1 == bind(sock_fd, (sockaddr*)&server_addr, sizeof(sockaddr_in))) {
    const int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }

  // Set socket to active mode.
  if (-1 == listen(sock_fd, backlog)) {
    const int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }

  return sock_fd;
}

int tcp_accept(const int sock_fd, Connection* const connection) {
  // Accept a connection.
  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  int peer_sock_fd = accept(sock_fd, (sockaddr*)&client_addr, &addr_len);
  if (peer_sock_fd == -1) return -1;
  if (addr_len != sizeof(client_addr)) {
    errno = ENOANO;
    return -1;
  }

  // Get the server's address.
  struct sockaddr_in server_addr;
  addr_len = sizeof(server_addr);
  if (-1 == getsockname(peer_sock_fd, (sockaddr*)&server_addr, &addr_len)) {
    int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }
  if (addr_len != sizeof(server_addr)) {
    errno = ENOANO;
    return -1;
  }

  connection->sock_fd = peer_sock_fd;
  connection->server_ip   = server_addr.sin_addr.s_addr;
  connection->server_port = server_addr.sin_port;
  connection->client_ip   = client_addr.sin_addr.s_addr;
  connection->client_port = client_addr.sin_port;
  return 0;
}

int readn(int sock_fd, void* buf, size_t n_bytes) {
  uint8_t* buff = (uint8_t*)buf;
  while (n_bytes > 0) {
    const auto ret = read(sock_fd, buff, n_bytes);
    // TODO: be more selective about which errors are fatal
    if (ret <= 0) return -1;
    n_bytes -= ret;
    buff    += ret;
  }
  return 0;
}

