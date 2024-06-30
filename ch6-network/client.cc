#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

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

  int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (-1 == sock_fd) {
    perror("failed to open socket");
    exit(1);
  }

  sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(args.port);
  if (-1 == inet_pton(AF_INET, args.server, &server_addr.sin_addr)) {
    char* errstr = strerror(errno);
    fprintf(stderr, "failed to parse server ip: \"%s\"\n", args.server);
    exit(1);
  }

  printf("connecting to port %d\n", args.port);

  if (-1 == connect(sock_fd, (sockaddr*)&server_addr, sizeof(server_addr))) {
    perror("failed to connect");
    exit(1);
  }
  printf("connected!\n");

  RPCMessage message;
  message.mark.signature = MARK_SIGNATURE;
  message.mark.header_len = sizeof(RPCHeader);
  message.mark.data_len = 0;
  message.mark.checksum = 0xfadedafb;

  if (-1 == write(sock_fd, &message, sizeof(RPCMark))) {
    perror("failed to write");
    exit(1);
  }
  // TODO: send the rest of the message

  close(sock_fd);
  return 0;
}

