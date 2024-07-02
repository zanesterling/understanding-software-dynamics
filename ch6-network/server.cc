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

void handle_rpc_ping(const Connection* const connection, RPCMessage* request) {
  rpc_send_resp(
    connection,
    request,
    request->body,
    request->mark.data_len,
    RPC_STATUS_OK
  );
}

void handle_rpc_write(const Connection* const connection);
void handle_rpc_read(const Connection* const connection);
void handle_rpc_chksum(const Connection* const connection);
void handle_rpc_delete(const Connection* const connection);
void handle_rpc_stats(const Connection* const connection);
void handle_rpc_reset(const Connection* const connection);
void handle_rpc_quit(const Connection* const connection);

RpcAction handle_rpc_conn(const Connection* const connection) {
  const int sock_fd = connection->sock_fd;
  const uint16_t port = connection->server_port;

  while (true) {
    RPCMessage message;
    // Read RPC mark.
    if (-1 == readn(sock_fd, (void*)&message.mark, sizeof(RPCMark))) {
      fprintf(stderr, "%d: failed to read: %m", port);
      return RpcAction::CONTINUE;
    }

    // Read RPC header.
    if (message.mark.header_len != sizeof(RPCHeader)) {
      fprintf(
        stderr, "%d: bad header len; expected %lu got %d\n",
        port, sizeof(RPCHeader), message.mark.header_len
      );
      return RpcAction::CONTINUE;
    }
    if (-1 == readn(sock_fd, (void*)&message.header, sizeof(RPCHeader))) {
      fprintf(stderr, "%d: failed to read: %m", port);
      return RpcAction::CONTINUE;
    }

    // Read RPC body.
    message.body = (uint8_t*) malloc(message.mark.data_len);
    if (-1 == readn(sock_fd, (void*)message.body, message.mark.data_len)) {
      fprintf(stderr, "%d: failed to read: %m", port);
      return RpcAction::CONTINUE;
    }
    if (-1 == now_usec(&message.header.req_recv_time_us)) {
      fprintf(stderr, "%d: failed to get time: %m", port);
      return RpcAction::CONTINUE;
    }

    printf("%d ", port);
    message.pretty_print();

    // Process command.
    if (strncmp(message.header.method, "ping", 8) == 0) {
      handle_rpc_ping(connection, &message);
    } else {
      fprintf(stderr, "%d: unrecognized command \"%.8s\"", port, message.header.method);
      return RpcAction::CONTINUE;
    }

    // On quit(), close socket.
    free(message.body);
    break;
  }

  printf("ending connection\n");
  return RpcAction::CONTINUE;
}

void* rpc_listen(void* void_args) {
  const ListenArgs* args = (ListenArgs*)void_args;

  int listen_sock_fd = tcp_listen(args->port, SOCK_BACKLOG);
  if (-1 == listen_sock_fd) {
    char* errstr = strerror(errno);
    fprintf(stderr, "%d: couldn't open listening socket: %s\n", args->port, errstr);
    return NULL;
  }
  printf("%d: listening!\n", args->port);

  while (true) {
    Connection connection;
    tcp_accept(listen_sock_fd, &connection);
    printf(
      "%d: accepted connection from %d.%d.%d.%d:%d to %d.%d.%d.%d:%d\n",
      args->port,
      (connection.client_ip & 0xff000000) >> 24,
      (connection.client_ip & 0x00ff0000) >> 16,
      (connection.client_ip & 0x0000ff00) >> 8,
       connection.client_ip & 0x000000ff,
      connection.client_port,
      (connection.server_ip & 0xff000000) >> 24,
      (connection.server_ip & 0x00ff0000) >> 16,
      (connection.server_ip & 0x0000ff00) >> 8,
       connection.server_ip & 0x000000ff,
      connection.server_port
    );

    // Handle as many RPCs as they send.
    const auto action = handle_rpc_conn(&connection);
    close(connection.sock_fd);
    // TODO: Actually, quit() should kill the whole server.
    if (action == RpcAction::QUIT) break;
  }

  printf("%d: closing, goodbye!\n", args->port);
  close(listen_sock_fd);
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
