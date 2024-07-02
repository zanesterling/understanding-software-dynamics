#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "network.h"
#include "rpc.h"

struct Args {
  char* server;
  uint16_t port;
  uint32_t reps = 1;
  uint32_t k = 1;
  uint32_t wait_ms = 0;
  bool seed1 = false;
  bool verbose = false;
  char* command;
  // TODO: key config
  // TODO: value config
};

void usage() {
  fprintf(stderr, "bad usage\n");
  // TODO
}

Args parse_args(int argc, char** argv) {
  Args args;
  // TODO: parse the rest of the arguments
  if (argc != 4) {
    usage();
    exit(1);
  }
  args.server = argv[1];
  errno = 0;
  args.port = strtol(argv[2], NULL, 10);
  if (errno != 0) {
    const char* errstr = strerror(errno);
    fprintf(stderr, "error: could not parse port from \"%s\": %s", argv[2], errstr);
  }
  args.command = argv[3];
  return args;
}

int main(int argc, char** argv) {
  Args args = parse_args(argc, argv);

  Connection connection;
  if (-1 == tcp_connect(args.server, args.port, &connection)) {
    char* errstr = strerror(errno);
    fprintf(stderr, "failed to connect to %s:%d: %s\n",
            args.server, args.port, errstr);
    exit(1);
  }

  printf(
    "client connected from %d.%d.%d.%d:%d to %d.%d.%d.%d:%d\n",
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

  const uint8_t* body = (uint8_t*)"foo bar baz";
  size_t n_bytes = 12;
  rpc_send_req(&connection, body, n_bytes, /*parent_rpc=*/0, "ping");

  RPCMessage response;
  if (-1 == rpc_recv_resp(&connection, &response)) {
    printf("failed to receive the response: %m\n");
  }
  response.pretty_print();

  close(connection.sock_fd);
  return 0;
}

