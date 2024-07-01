#include "network.h"

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

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

