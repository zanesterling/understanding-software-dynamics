#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "rpc.h"

struct ListenArgs {
  const int port;

  ListenArgs(int port) : port(port) {}
};

void* listen(void* void_args) {
  ListenArgs* args = (ListenArgs*)void_args;
  printf("%d: listening\n", args->port);

  // Open socket.
  // Read RPC command.
  // Process command.
  // On quit(), close socket.

  printf("%d: closing, goodbye!\n", args->port);
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

  printf("Starting listen() threads for port ids [%d,%d].\n", start_port, end_port);

  const int n_threads = end_port - start_port + 1;
  pthread_t* thread_ids = (pthread_t*) malloc(sizeof(pthread_t) * n_threads);
  for (int i = 0; i < n_threads; ++i) {
    int port = start_port + i;
    printf("main: start thread for port %d\n", port);
    if (0 != pthread_create(&thread_ids[i], NULL, listen, new ListenArgs(port))) { // error
      perror("couldn't spawn the requested number of listen() threads");
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
