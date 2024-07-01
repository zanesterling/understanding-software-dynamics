#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "network.h"
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
    "\tclient:  %d.%d.%d.%d:%d\n"
    "\tserver:  %d.%d.%d.%d:%d\n"
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

    (client_ip & 0xff000000) >> 24,
    (client_ip & 0x00ff0000) >> 16,
    (client_ip & 0x0000ff00) >> 8,
     client_ip & 0x000000ff,
    client_port,
    (server_ip & 0xff000000) >> 24,
    (server_ip & 0x00ff0000) >> 16,
    (server_ip & 0x0000ff00) >> 8,
     server_ip & 0x000000ff,
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

uint32_t next_rpc_id = 0;

static inline uint8_t ilog2(const uint32_t x) {
  uint32_t y;
  asm("\tbsr %1, %0\n"
     : "=r"(y)
     : "r" (x)
  );
  return y;
}

int rpc_send_req(
  const Connection* const connection,
  const uint8_t* const body,
  const size_t n_bytes,
  const uint32_t parent_rpc,
  const char* const method
) {
  RPCMessage message;
  message.mark.signature = MARK_SIGNATURE;
  message.mark.header_len = sizeof(RPCHeader);
  message.mark.data_len = n_bytes;
  message.mark.checksum;
  message.header.rpc_id = next_rpc_id++;
  message.header.parent = parent_rpc;

  struct timeval tv;
  if (-1 == gettimeofday(&tv, NULL)) return -1;
  message.header.req_send_time_us = tv.tv_sec * 1000 * 1000 + tv.tv_usec;
  message.header.req_recv_time_us = 0;
  message.header.res_send_time_us = 0;
  message.header.res_recv_time_us = 0;

  message.header.client_ip   = connection->client_ip;
  message.header.client_port = connection->client_port;
  message.header.server_ip   = connection->server_ip;
  message.header.server_port = connection->server_port;
  message.header.req_len_log = ilog2(n_bytes + sizeof(RPCMessage));
  message.header.message_type = TYPE_REQUEST;
  strncpy(message.header.method, method, 8);

  size_t written_bytes = write(connection->sock_fd, &message, sizeof(RPCMessage));
  if (written_bytes != sizeof(RPCMessage)) {
    return -1;
  }
  written_bytes = write(connection->sock_fd, body, n_bytes);
  if (written_bytes != n_bytes) {
    return -1;
  }
  return 0;
}

