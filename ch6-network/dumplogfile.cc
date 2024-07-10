#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "rpc.h"

void usage(char** argv) {
  fprintf(
    stderr,
    "usage:\n"
    "\t%s LOG_FILE\n",
    argv[0]
  );
}

struct LogMessage {
  RPCHeader header;
  uint8_t body[24];
};

static_assert(96 == sizeof(LogMessage));

int main(int argc, char** argv) {
  if (argc != 2) usage(argv), exit(1);

  int log_fd = open(argv[1], O_RDONLY);
  if (-1 == log_fd) {
    fprintf(stderr, "failed to open logfile \"%s\": %m", argv[1]);
    exit(1);
  }

  // Logs are arranged as MARK HEADER BODY_TRUNC,
  // where BODY_TRUNC is 24 bytes of truncated or zero-extended data.

  while (true) {
    LogMessage message;
    ssize_t n_bytes = read(log_fd, &message, sizeof(LogMessage));
    if (-1 == n_bytes) {
      perror("failed to read log message from file");
      exit(1);
    }
    if (0 == n_bytes) break; // EOF.
    if (n_bytes != sizeof(LogMessage)) {
      fprintf(
        stderr,
        "couldn't read full message from file. got %ld bytes instead\n", 
        n_bytes
      );
      exit(1);
    }

    message.header.pretty_print();
    printf("body: %s\n\n", message.body);
  }

  return 0;
}
