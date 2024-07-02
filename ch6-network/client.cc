#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "network.h"
#include "rpc.h"

struct KeyConfig {
  const char* key_base;
  bool increment_base;
  size_t padded_length;
};

struct ValueConfig {
  const char* value_base;
  bool increment_base;
  size_t padded_length;
};

struct Args {
  char* server;
  uint16_t port;
  uint32_t n_conns = 1;
  uint32_t rpcs_per_conn = 1;
  uint32_t wait_ms = 0;
  bool seed1 = false;
  bool verbose = false;
  char* command;
  KeyConfig key_config;
  ValueConfig value_config;
};

void usage() {
  fprintf(stderr, "bad usage\n");
  // TODO
}

Args parse_args(int argc, char** argv) {
  Args args;
  if (argc < 4) usage(), exit(1);
  args.server = argv[1];

  errno = 0;
  args.port = strtol(argv[2], NULL, 10);
  if (errno != 0) {
    const char* errstr = strerror(errno);
    fprintf(stderr, "error: could not parse port from \"%s\": %s", argv[2], errstr);
    usage();
    exit(1);
  }

  int next_arg = 3;
  for (; next_arg < argc; ++next_arg) {
    if (strcmp("-rep", argv[next_arg]) == 0) {
      if (next_arg+1 >= argc) usage(), exit(1);
      args.n_conns = atoi(argv[next_arg+1]);
      ++next_arg;
    } else if (strcmp("-k", argv[next_arg]) == 0) {
      if (next_arg+1 >= argc) usage(), exit(1);
      args.rpcs_per_conn = atoi(argv[next_arg+1]);
      ++next_arg;
    } else if (strcmp("-waitms", argv[next_arg]) == 0) {
      if (next_arg+1 >= argc) usage(), exit(1);
      args.wait_ms = atoi(argv[next_arg+1]);
      ++next_arg;
    } else if (strcmp("-seed1", argv[next_arg]) == 0) {
      args.seed1 = true;
    } else if (strcmp("-verbose", argv[next_arg]) == 0) {
      args.verbose = true;
    } else {
      // This must be the command! We'll handle it and the other two flags
      // separately.
      break;
    }
  }

  if (next_arg >= argc) usage(), exit(1);
  args.command = argv[next_arg++];

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

