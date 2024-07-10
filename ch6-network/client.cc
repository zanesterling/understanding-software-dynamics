#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "my_rpc.h"
#include "network.h"
#include "rpc.h"

struct StrConfig {
  const char* base = NULL;
  bool increment_base = false;
  size_t padded_length = 0;
};

enum class Command {
  Ping,
  Write,
  Read,
  Quit
};

struct Args {
  char* server;
  uint16_t port;
  uint32_t n_conns = 1;
  uint32_t rpcs_per_conn = 1;
  uint32_t wait_ms = 0;
  bool seed1 = false;
  bool verbose = false;
  Command command;
  char* command_str;
  StrConfig key_config;
  StrConfig value_config;
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
  args.command_str = argv[next_arg++];
  if (strcmp(args.command_str, "ping") == 0) {
    args.command = Command::Ping;
  } else if (strcmp(args.command_str, "write") == 0) {
    args.command = Command::Write;
  } else if (strcmp(args.command_str, "read") == 0) {
    args.command = Command::Read;
  } else if (strcmp(args.command_str, "quit") == 0) {
    args.command = Command::Quit;
  } else {
    fprintf(stderr, "unrecognized command: \"%s\"\n", args.command_str);
    usage();
    exit(1);
  }

  for (; next_arg < argc; ++next_arg) {
    // TODO: factor out common logic.
    if (strcmp("-key", argv[next_arg]) == 0) {
      if (next_arg+1 >= argc) usage(), exit(1);
      args.key_config.base = argv[next_arg+1];
      next_arg += 2;
      if (next_arg < argc && strcmp(argv[next_arg], "+") == 0) {
        args.key_config.increment_base = true;
        ++next_arg;
      }
      if (next_arg < argc && argv[next_arg][0] != '-') {
        args.key_config.padded_length = strtol(argv[next_arg+3], NULL, 10);
        ++next_arg;
      } else {
        args.key_config.padded_length = strlen(args.key_config.base);
      }
      --next_arg;
      continue;
    } else if (strcmp("-value", argv[next_arg]) == 0) {
      if (next_arg+1 >= argc) usage(), exit(1);
      args.value_config.base = argv[next_arg+1];
      next_arg += 2;
      if (next_arg < argc && strcmp(argv[next_arg], "+") == 0) {
        args.value_config.increment_base = true;
        ++next_arg;
      }
      if (next_arg < argc && argv[next_arg][0] != '-') {
        args.value_config.padded_length = strtol(argv[next_arg+3], NULL, 10);
        ++next_arg;
      } else {
        args.value_config.padded_length = strlen(args.value_config.base);
      }
      --next_arg;
      continue;
    } else {
      fprintf(stderr, "unrecognized arg: %s\n", argv[next_arg]);
      usage();
      exit(1);
    }
  }

  // Done parsing, let's validate.
  if (Command::Write == args.command) {
    bool fail = false;
    if (NULL == args.key_config.base) {
      fprintf(stderr, "write command expects -key arg\n");
      fail = true;
    }
    if (NULL == args.value_config.base) {
      fprintf(stderr, "write command expects -value arg\n");
      fail = true;
    }
    if (fail) exit(1);
  }

  return args;
}

// Produce the nth string according to the given StrConfig.
void gen_str_direct(
  const StrConfig* const config,
  const int n,
  size_t* const length,
  char** const data
) {
  if (NULL == config->base) {
    *length = 12;
    *data = (char*)malloc(*length);
    memcpy(*data, "foo bar baz", *length);
    return;
  }
  // TODO: full gen logic include increment and padlen.
  *length = strlen(config->base);
  *data = (char*)malloc(*length);
  memcpy(*data, config->base, *length);
}

// Produce the nth string according to the given StrConfig.
LenStr gen_str(const StrConfig* const config, const int n) {
  size_t length;
  char* data;
  gen_str_direct(config, n, &length, &data);
  return LenStr(length, data);
}

const char* const log_fn = "client.log";

int main(int argc, char** argv) {
  Args args = parse_args(argc, argv);

  // Open logfile.
  constexpr mode_t RW_MODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
  int log_fd = creat(log_fn, RW_MODE);
  if (-1 == log_fd) {
    fprintf(stderr, "failed to open log file \"%s\": %m\n", log_fn);
    exit(1);
  }

  for (unsigned int i = 0; i < args.n_conns; ++i) {
    Connection connection;
    if (-1 == tcp_connect(args.server, args.port, &connection)) {
      char* errstr = strerror(errno);
      fprintf(stderr, "failed to connect to %s:%d: %s\n",
              args.server, args.port, errstr);
      exit(1);
    }

    if (args.verbose) {
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
    }

    for (unsigned int j = 0; j < args.rpcs_per_conn; ++j) {
      uint8_t* body = NULL;
      size_t n_bytes = 0;
      switch (args.command) {
        case Command::Ping:
          gen_str_direct(&args.value_config, j, &n_bytes, (char**)&body);
          break;

        case Command::Quit:
          break;

        case Command::Write: {
          LenStr key   = gen_str(&args.key_config,   j); // TODO
          LenStr value = gen_str(&args.value_config, j);
          const auto write_req = WriteRequest::Make(
            key.data,   key.length,
            value.data, value.length
          );
          body = (uint8_t*) write_req;
          n_bytes = write_req->full_len();
          break;
        }

        case Command::Read: {
          gen_str_direct(&args.key_config, j, &n_bytes, (char**)&body);
          break;
        }

        default:
          fprintf(stderr, "unrecognized command: \"%s\"\n", args.command_str);
          exit(1);
      }
      rpc_send_req(&connection, body, n_bytes, /*parent_rpc=*/0, args.command_str, log_fd);
      free(body);

      RPCMessage response;
      if (-1 == rpc_recv_resp(&connection, &response)) {
        fprintf(stderr, "failed to receive the response: %m\n");
        exit(1);
      }
      log(log_fd, &response);
      if (args.verbose) response.pretty_print();
      free(response.body);

      uint64_t now;
      do {
        if (-1 == now_usec(&now)) {
          fprintf(stderr, "failed to get current time: %m\n");
          exit(1);
        }
      } while (now - response.header.res_recv_time_us < args.wait_ms * 1000);
    }

    close(connection.sock_fd);
  }

  return 0;
}

