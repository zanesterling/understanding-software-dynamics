#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "network.h"
#include "rpc.h"

#define hton16 htons
#define hton32 htonl
#define ntoh16 ntohs
#define ntoh32 ntohl

struct ListenArgs {
  const int port;

  ListenArgs(int port) : port(port) {}
};

const int SOCK_BACKLOG = 1;

enum class RpcAction {
  QUIT,
  CONTINUE,
};

void handle_rpc_ping();
void handle_rpc_write();
void handle_rpc_read();
void handle_rpc_chksum();
void handle_rpc_delete();
void handle_rpc_stats();
void handle_rpc_reset();
void handle_rpc_quit();

RpcAction handle_rpc_conn(int port, int peer_sock_fd) {
  while (true) {
    // Read RPC mark.
    printf("%d: reading rpc mark\n", port);
    RPCMark rpc_mark;
    size_t read_bytes = 0;
    while (read_bytes < sizeof(RPCMark)) {
      void* const buf = (void*) (read_bytes + (uint8_t*)&rpc_mark);
      const auto ret = read(peer_sock_fd, buf, sizeof(RPCMark) - read_bytes);
      if (ret >= 0) read_bytes += ret;
      else {
        char* errstr = strerror(errno);
        fprintf(stderr, "%d: failed to read: %s", port, errstr);
        close(peer_sock_fd);
        // TODO: be more selective about which errors are fatal
      }
    }
    printf("%d: ", port);
    rpc_mark.pretty_print();

    // Read RPC header.
    printf("%d: reading rpc header\n", port);
    RPCHeader rpc_header;
    read_bytes = 0;
    while (read_bytes < rpc_mark.header_len) {
      void* const buf = (void*) (read_bytes + (uint8_t*)&rpc_header);
      const auto ret = read(peer_sock_fd, buf, rpc_mark.header_len - read_bytes);
      if (ret >= 0) read_bytes += ret;
      else {
        char* errstr = strerror(errno);

        fprintf(stderr, "%d: failed to read: %s", port, errstr);
        close(peer_sock_fd);
        // TODO: be more selective about which errors are fatal
      }
    }
    printf("%d ", port);
    rpc_header.pretty_print();

    // Read RPC body.
    // Process command.
    // On quit(), close socket.
    break;
  }

  printf("ending connection\n");
  return RpcAction::CONTINUE;
}

void* rpc_listen(void* void_args) {
  const ListenArgs* args = (ListenArgs*)void_args;

  int sock_fd = tcp_listen(args->port, SOCK_BACKLOG);
  if (-1 == sock_fd) {
    char* errstr = strerror(errno);
    fprintf(stderr, "%d: couldn't open listening socket: %s\n", args->port, errstr);
    return NULL;
  }
  printf("%d: listening!\n", args->port);

  while (true) {
    // Accept a connection.
    printf("%d: accepting...\n", args->port);
    struct sockaddr client_addr;
    socklen_t addr_len;
    int peer_sock_fd = accept(sock_fd, &client_addr, &addr_len);
    if (peer_sock_fd == -1) {
      char* errstr = strerror(errno);
      fprintf(stderr, "%d: couldn't accept: %s\n", args->port, errstr);
      continue;
    }

    // Handle as many RPCs as they send.
    const auto action = handle_rpc_conn(args->port, peer_sock_fd);
    close(peer_sock_fd);
    // TODO: Actually, quit() should kill the whole server.
    if (action == RpcAction::QUIT) break;
  }

  printf("%d: closing, goodbye!\n", args->port);
  close(sock_fd);
  return NULL;
}

void usage(FILE* fd, char* argv0) {
  fprintf(
    fd,
    "usage:\n"
    "\t%s [START_PORT END_PORT]\n"
    "\n"
    "Opens a dedicated listening thread for each port in the inclusive range"
    " [START_PORT, END_PORT].\n"
    "START_PORT defaults to 12345.\n"
    "END_PORT defaults to 12348.\n",
    argv0
  );
}

int main(int argc, char** argv) {
  if (argc != 1 && argc != 3) {
    usage(stderr, argv[0]);
    exit(1);
  }

  int start_port = 12345;
  int end_port   = 12348;
  if (argc == 3) {
    start_port = atoi(argv[1]);
    end_port   = atoi(argv[2]);
  }
  if (end_port < start_port) {
    fprintf(
      stderr,
      "err: END_PORT must not be less than START_PORT\n"
      "but got start=%d end=%d\n",
      start_port, end_port
    );
    usage(stderr, argv[0]);
    exit(1);
  }

  printf("Starting rpc_listen() threads for port ids [%d,%d].\n", start_port, end_port);

  const int n_threads = end_port - start_port + 1;
  pthread_t* thread_ids = (pthread_t*) malloc(sizeof(pthread_t) * n_threads);
  for (int i = 0; i < n_threads; ++i) {
    int port = start_port + i;
    printf("main: start thread for port %d\n", port);
    if (0 != pthread_create(&thread_ids[i], NULL, rpc_listen, new ListenArgs(port))) { // error
      perror("couldn't spawn the requested number of rpc_listen() threads");
      exit(1);
      // TODO: Will the child threads properly clean up their sockets on exit?
    }
  }

  for (int i = 0; i < n_threads; ++i) {
    void* retval;
    if (0 != pthread_join(thread_ids[i], &retval)) {
      perror("couldn't join thread\n");
    }
    printf("main: joined thread for port %d\n", start_port + i);
  }

  return 0;
}
