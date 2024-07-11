#include <assert.h>
#include <inttypes.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
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

void myputs(FILE* fd, const char* s) {
  for (; *s; ++s) fputc(*s, fd);
}

void fhexdumpn(FILE* fd, const char* s, size_t n) {
  for (size_t i = 0; i < n; i += 8) {
    for (size_t j = 0; j < 8; ++j) {
      if (i+j >= n) {
        myputs(fd, "   ");
      } else {
        fprintf(fd, "%02x ", s[i+j]);
      }
    }
    fputc('|', fd);
    for (size_t j = 0; j < 8 && i+j < n; ++j) {
      if (j < 0x20 || j > 0x7e) {
        // Nonprintable character, so print a '.'.
        fputc('.', fd);
      } else {
        fputc(s[i+j], fd);
      }
    }
    myputs(fd, "|\\n");
  }
}

int main(int argc, char** argv) {
  if (argc != 2) usage(argv), exit(1);

  int log_fd = open(argv[1], O_RDONLY);
  if (-1 == log_fd) {
    fprintf(stderr, "failed to open logfile \"%s\": %m", argv[1]);
    exit(1);
  }

  // Logs are arranged as MARK HEADER BODY_TRUNC,
  // where BODY_TRUNC is 24 bytes of truncated or zero-extended data.

  putchar('[');
  bool is_first_message = true;
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

    if (!is_first_message) {
      myputs(stdout, ",\n");
    }
    is_first_message = false;

    printf(
      "{\n"
      "\t\"rpc_id\":      %d,\n"
      "\t\"parent\":      %d,\n"
      "\t\"t1_us\":       %" PRIu64 ",\n"
      "\t\"t2_us\":       %" PRIu64 ",\n"
      "\t\"t3_us\":       %" PRIu64 ",\n"
      "\t\"t4_us\":       %" PRIu64 ",\n"
      "\t\"client\":      \"%d.%d.%d.%d:%d\",\n"
      "\t\"server\":      \"%d.%d.%d.%d:%d\",\n"
      "\t\"req_len_log\": %u,\n"
      "\t\"res_len_log\": %u,\n",

      message.header.rpc_id,
      message.header.parent,
      message.header.req_send_time_us,
      message.header.req_recv_time_us,
      message.header.res_send_time_us,
      message.header.res_recv_time_us,

      (message.header.client_ip & 0xff000000) >> 24,
      (message.header.client_ip & 0x00ff0000) >> 16,
      (message.header.client_ip & 0x0000ff00) >> 8,
       message.header.client_ip & 0x000000ff,
       message.header.client_port,
      (message.header.server_ip & 0xff000000) >> 24,
      (message.header.server_ip & 0x00ff0000) >> 16,
      (message.header.server_ip & 0x0000ff00) >> 8,
       message.header.server_ip & 0x000000ff,
       message.header.server_port,

      message.header.req_len_log,
      message.header.res_len_log
    );

    myputs(stdout, "\t\"type\":     \"");
    const char* type_str = message_type_str(message.header.message_type);
    fhexdumpn(stdout, type_str, strlen(type_str));
    myputs(stdout, "\",\n");

    myputs(stdout, "\t\"method\":   \"");
    fhexdumpn(stdout, message.header.method, 8);
    myputs(stdout, "\",\n");

    myputs(stdout, "\t\"status\":   \"");
    const char* stat_str = status_str(message.header.status);
    fhexdumpn(stdout, stat_str, strlen(stat_str));
    myputs(stdout, "\",\n");

    myputs(stdout, "\t\"body\":     \"");
    fhexdumpn(stdout, (char*)message.body, 24);
    myputs(stdout, "\"\n");

    myputs(stdout, "}");
  }
  myputs(stdout, "]\n");

  return 0;
}
