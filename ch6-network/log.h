#pragma once

#include "rpc.h"

// Logs the mark, header, and the first few bytes of the body, if present.
int log(int log_fd, const RPCMessage* message);

