#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

#include "log.h"
#include "my_rpc.h"
#include "network.h"
#include "rpc.h"
#include "spinlock.h"

#define hton16 htons
#define hton32 htonl
#define ntoh16 ntohs
#define ntoh32 ntohl

bool verbose = false;
#define VERBOSE(x) if (verbose) { x; }

LockAndHist lock;
std::unordered_map<std::string, std::string> keystore;

struct ListenArgs {
  const int port;

  ListenArgs(int port) : port(port) {}
};

const int SOCK_BACKLOG = 1;

enum class RpcAction {
  QUIT,
  CONTINUE,
};

void handle_rpc_ping(
  const Connection* const connection,
  const RPCMessage* const request,
  const int log_fd
) {
  // Echo the request back to the client.
  rpc_send_resp(
    connection,
    request,
    request->body,
    request->mark.data_len,
    RpcStatus::Ok,
    log_fd
  );
}

void handle_rpc_write(
  const Connection* const connection,
  const RPCMessage* const request,
  const int log_fd
) {
  WriteRequest* write_req = WriteRequest::FromBody(request->body, request->mark.data_len);
  if (NULL == write_req) {
    fprintf(
      stderr, "%d: failed to parse write request\n",
      connection->server_port
    );
    rpc_send_resp(connection, request, NULL, 0, RpcStatus::BadArg, log_fd);
    return;
  }
  std::string key(write_req->key(), write_req->key_len());
  std::string value(write_req->value(), write_req->value_len());

  {
    SpinLock spinlock(&lock);
    keystore.emplace(key, value);
  }

  rpc_send_resp(
    connection,
    request,
    NULL,
    0,
    RpcStatus::Ok,
    log_fd
  );
}

void handle_rpc_read(
  const Connection* const connection,
  const RPCMessage* const request,
  const int log_fd
) {
  const std::string key((char*)request->body, request->mark.data_len);

  bool found;
  std::string result;
  {
    SpinLock spinlock(&lock);
    const auto iter = keystore.find(key);
    if (iter == keystore.end()) {
      found = false;
    } else {
      found = true;
      result = iter->second;
    }
  }

  if (found) {
    rpc_send_resp(
      connection,
      request,
      (uint8_t*) result.c_str(),
      result.size(),
      RpcStatus::Ok,
      log_fd
    );
  } else {
    rpc_send_resp(
      connection,
      request,
      NULL,
      0,
      RpcStatus::NotFound,
      log_fd
    );
  }
}

void handle_rpc_chksum(const Connection* const connection);
void handle_rpc_delete(const Connection* const connection);
void handle_rpc_stats(const Connection* const connection);
void handle_rpc_reset(const Connection* const connection);

RpcAction handle_rpc_conn(
  const Connection* const connection,
  const int log_fd
) {
  const uint16_t port = connection->server_port;

  while (true) {
    VERBOSE(printf("%d: listening for message\n", port));
    RPCMessage message;
    if (-1 == rpc_recv_req(connection, &message)) break;
    log(log_fd, &message);
    VERBOSE({
      printf("%d ", port);
      message.pretty_print();
    });

    // Process command.
    if (strncmp(message.header.method, "ping", 8) == 0) {
      handle_rpc_ping(connection, &message, log_fd);
    } else if (strncmp(message.header.method, "write", 8) == 0) {
      handle_rpc_write(connection, &message, log_fd);
    } else if (strncmp(message.header.method, "read", 8) == 0) {
      handle_rpc_read(connection, &message, log_fd);
    } else if (strncmp(message.header.method, "quit", 8) == 0) {
      // On quit(), close socket.
      rpc_send_resp(
        connection,
        &message,
        NULL,
        0,
        RpcStatus::Ok,
        log_fd
      );
      free(message.body);
      return RpcAction::QUIT;
    } else {
      fprintf(stderr, "%d: unrecognized command \"%.8s\"", port, message.header.method);
      return RpcAction::CONTINUE;
    }

    free(message.body);
  }

  VERBOSE(printf("ending connection\n"));
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
  VERBOSE(printf("%d: listening!\n", args->port));

  char log_fn[128];
  snprintf(log_fn, 128, "server-%d.log", args->port);
  constexpr mode_t RW_MODE = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
  int log_fd = creat(log_fn, RW_MODE);
  if (-1 == log_fd) {
    fprintf(stderr, "failed to open log file \"%s\": %m\n", log_fn);
    exit(1);
  }

  while (true) {
    Connection connection;
    tcp_accept(listen_sock_fd, &connection);
    VERBOSE(printf(
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
    ));

    // Handle as many RPCs as they send.
    const auto action = handle_rpc_conn(&connection, log_fd);
    close(connection.sock_fd);
    // TODO: Actually, quit() should kill the whole server.
    if (action == RpcAction::QUIT) break;
  }

  VERBOSE(printf("%d: closing, goodbye!\n", args->port));
  close(listen_sock_fd);
  close(log_fd);
  return NULL;
}

void usage(FILE* fd, const char* argv0) {
  fprintf(
    fd,
    "usage:\n"
    "\t%s [-v] [START_PORT END_PORT]\n"
    "\n"
    "Opens a dedicated listening thread for each port in the inclusive range"
    " [START_PORT, END_PORT].\n"
    "START_PORT defaults to 12345.\n"
    "END_PORT defaults to 12348.\n",
    argv0
  );
}

struct Args {
  bool verbose = false;
  int start_port;
  int end_port;
};

Args parse_args(int argc, const char* const* argv) {
  Args args;

  const char* bin_name = argv[0];
  argc--; argv++;

  if (argc > 0 && strcmp(argv[0], "-v") == 0) {
    args.verbose = true;
    argc--; argv++;
  }

  if (argc == 0) {
    args.start_port = 12345;
    args.end_port   = 12348;
  } else if (argc == 2) {
    args.start_port = atoi(argv[1]);
    args.end_port   = atoi(argv[2]);
  } else {
    usage(stderr, bin_name);
    exit(1);
  }

  if (args.end_port < args.start_port) {
    fprintf(
      stderr,
      "err: END_PORT must not be less than START_PORT\n"
      "but got start=%d end=%d\n",
      args.start_port,
      args.end_port
    );
    usage(stderr, bin_name);
    exit(1);
  }

  return args;
}

int main(int argc, char** argv) {
  Args args = parse_args(argc, argv);
  verbose = args.verbose;
  memset(&lock, 0, sizeof(LockAndHist));

  VERBOSE(printf(
    "Starting rpc_listen() threads for port ids [%d,%d].\n",
    args.start_port,
    args.end_port
  ));

  const int n_threads = args.end_port - args.start_port + 1;
  pthread_t* thread_ids = (pthread_t*) malloc(sizeof(pthread_t) * n_threads);
  for (int i = 0; i < n_threads; ++i) {
    int port = args.start_port + i;
    VERBOSE(printf("main: start thread for port %d\n", port));
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
    VERBOSE(printf("main: joined thread for port %d\n", args.start_port + i));
  }

  VERBOSE(puts("main: last thread joined; terminating\n"));

  return 0;
}
