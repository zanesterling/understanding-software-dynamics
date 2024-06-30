#include "rpc.h"

uint8_t* RPCMessage::data() {
  return sizeof(RPCMessage) + (uint8_t*) this;
}

