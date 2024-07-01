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

int tcp_listen(
  const uint16_t port,
  const int backlog,
  Connection* const out_conn
) {
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

  // Get the server's address.
  socklen_t addr_len = sizeof(server_addr);
  if (-1 == getsockname(sock_fd, (sockaddr*)&server_addr, &addr_len)) {
    int err_save = errno;
    close(sock_fd);
    errno = err_save;
    return -1;
  }
  if (addr_len != sizeof(server_addr)) {
    errno = ENOANO;
    return -1;
  }

  out_conn->sock_fd = sock_fd;
  out_conn->server_ip   = server_addr.sin_addr.s_addr;
  out_conn->server_port = server_addr.sin_port;
  out_conn->client_ip   = 0;
  out_conn->client_port = 0;
  return 0;
}

