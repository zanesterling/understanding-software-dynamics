#include "log.h"

#include <string.h>
#include <sys/param.h>
#include <unistd.h>

// Logs the header and the first 24 bytes of the body, truncated or
// zero-extended to fit a length of 24.
int log(int log_fd, const RPCMessage* message) {
  if (-1 == write(log_fd, &message->header, sizeof(message->header))) {
    return -1;
  }
  uint8_t log_body[24];
  memset(log_body, 0, 24);
  if (message->body != NULL) {
    memcpy(log_body, message->body, MIN(24, message->mark.data_len));
  }
  return 0;
}

