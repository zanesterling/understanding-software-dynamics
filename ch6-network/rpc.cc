#include <stdio.h>
#include <unistd.h>

#include "rpc.h"

void RPCMark::pretty_print() {
  printf(
    "RPCMark:\n"
    "\tsignature:  0x%x\n"
    "\theader_len: %d\n"
    "\tdata_len:   %d\n"
    "\tchecksum:   0x%x\n",
    signature,
    header_len,
    data_len,
    checksum
  );
}

const char* message_type_str(uint16_t message_type) {
  switch (message_type) {
    case TYPE_REQUEST:  return "REQUEST";
    case TYPE_RESPONSE: return "RESPONSE";
    default:            return "UNRECOGNIZED";
  }
}

void RPCHeader::pretty_print() {
  printf(
    "RPCHeader:\n"
    "\trpc_id:  0x%x\n"
    "\tparent:  0x%x\n"
    "\tt1:      %lu\n"
    "\tt2:      %lu\n"
    "\tt3:      %lu\n"
    "\tt4:      %lu\n"
    "\tclient:  :%d\n" // TODO: print IP
    "\terver:   :%d\n" // TODO: print IP
    "\treq_len: 2^%u\n"
    "\tres_len: 2^%u\n"
    "\ttype:    %s\n"
    "\tmethod:  %.8s\n"
    "\tstatus:  %d\n",

    rpc_id,
    parent,
    req_send_time_us,
    req_recv_time_us,
    res_send_time_us,
    res_recv_time_us,

    // client_ip,
    // server_ip,
    client_port,
    server_port,

    req_len_log,
    res_len_log,

    message_type_str(message_type),
    method,
    status
  );
}

uint8_t* RPCMessage::data() {
  return sizeof(RPCMessage) + (uint8_t*) this;
}

int RPCMessage::send(int sock_fd) {
  size_t n_bytes = sizeof(RPCMark) + this->mark.header_len + this->mark.data_len;
  return write(sock_fd, this, n_bytes);
}

