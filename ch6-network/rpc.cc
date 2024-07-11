#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "log.h"
#include "print_hex.h"
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

const char* message_type_str(RpcMessageType message_type) {
  switch (message_type) {
    case RpcMessageType::Request:  return "REQUEST";
    case RpcMessageType::Response: return "RESPONSE";
    default:                       return "UNRECOGNIZED";
  }
}

const char* status_str(RpcStatus status) {
  switch (status) {
    case RpcStatus::Ok:       return "OK";
    case RpcStatus::BadArg:   return "BAD_ARG";
    case RpcStatus::NotFound: return "NOT_FOUND";
    default:                  return "UNRECOGNIZED";
  }
}

void RPCHeader::pretty_print() {
  printf(
    "RPCHeader:\n"
    "\trpc_id:  0x%x\n"
    "\tparent:  0x%x\n"
    "\tt1:      %" PRIu64 "s %" PRIu64 "us\n"
    "\tt2:      %" PRIu64 "s %" PRIu64 "us\n"
    "\tt3:      %" PRIu64 "s %" PRIu64 "us\n"
    "\tt4:      %" PRIu64 "s %" PRIu64 "us\n"
    "\tclient:  %d.%d.%d.%d:%d\n"
    "\tserver:  %d.%d.%d.%d:%d\n"
    "\treq_len: 2^%u\n"
    "\tres_len: 2^%u\n"
    "\ttype:    %s\n"
    "\tmethod:  %.8s\n"
    "\tstatus:  %s\n",

    rpc_id,
    parent,
    req_send_time_us / 1000000,
    req_send_time_us % 1000000,
    req_recv_time_us / 1000000,
    req_recv_time_us % 1000000,
    res_send_time_us / 1000000,
    res_send_time_us % 1000000,
    res_recv_time_us / 1000000,
    res_recv_time_us % 1000000,

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
    status_str(status)
  );
}

void RPCMessage::pretty_print() {
  this->mark.pretty_print();
  this->header.pretty_print();
  print_hex(this->body, this->mark.data_len);
}

int RPCMessage::send(int sock_fd) {
  size_t n_bytes = sizeof(RPCMark) + this->mark.header_len + this->mark.data_len;
  return write(sock_fd, this, n_bytes);
}

uint32_t next_rpc_id = 1;

static inline uint8_t ilog2(const uint32_t x) {
#ifdef __x86_64__
  uint32_t y;
  asm("\tbsr %1, %0\n"
     : "=r"(y)
     : "r" (x)
  );
  return y;
#else
	uint32_t ret = 0;
	uint32_t pow = 1;
	while (pow < x) {
		++ret;
		pow <<= 1;
	}
	return ret;
#endif
}

// If log_fd < 0, does not log.
int rpc_send_req(
  const Connection* const connection,
  const uint8_t* const body,
  const size_t n_bytes,
  const uint32_t parent_rpc,
  const char* const method,
  int log_fd
) {
  RPCMessage message;
  message.mark.signature = MARK_SIGNATURE;
  message.mark.header_len = sizeof(RPCHeader);
  message.mark.data_len = n_bytes;
  // TODO: Set message.mark.checksum.
  message.header.rpc_id = next_rpc_id++;
  message.header.parent = parent_rpc;

  if (-1 == now_usec(&message.header.req_send_time_us)) return -1;
  message.header.req_recv_time_us = 0;
  message.header.res_send_time_us = 0;
  message.header.res_recv_time_us = 0;

  message.header.client_ip   = connection->client_ip;
  message.header.client_port = connection->client_port;
  message.header.server_ip   = connection->server_ip;
  message.header.server_port = connection->server_port;

  size_t mark_and_header = sizeof(RPCMark) + sizeof(RPCHeader);
  message.header.req_len_log = ilog2(n_bytes + mark_and_header);
  message.header.message_type = RpcMessageType::Request;

  // GCC gets scared because we're leaving out the null byte.
  // Give it soothing headpats.
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wstringop-truncation"
  strncpy(message.header.method, method, 8);
  #pragma GCC diagnostic pop

  message.header.status = RpcStatus::Ok;

  size_t written_bytes = write(connection->sock_fd, &message, mark_and_header);
  if (written_bytes != mark_and_header) {
    return -1;
  }
  written_bytes = write(connection->sock_fd, body, n_bytes);
  if (written_bytes != n_bytes) {
    return -1;
  }

  if (log_fd >= 0) log(log_fd, &message);
  return 0;
}

int rpc_send_resp(
  const Connection* connection,
  const RPCMessage* request,
  uint8_t* body,
  size_t n_bytes,
  RpcStatus status,
  int log_fd
) {
  RPCMessage message;
  memcpy(&message, request, sizeof(RPCMessage));
  message.body = body;
  message.mark.data_len = n_bytes;
  // TODO: Set message.mark.checksum.

  if (-1 == now_usec(&message.header.res_send_time_us)) return -1;
  size_t mark_and_header = sizeof(RPCMark) + sizeof(RPCHeader);
  message.header.res_len_log = ilog2(n_bytes + mark_and_header);
  message.header.message_type = RpcMessageType::Response;
  message.header.status = status;

  size_t written_bytes = write(connection->sock_fd, &message, mark_and_header);
  if (written_bytes != mark_and_header) {
    return -1;
  }
  written_bytes = write(connection->sock_fd, body, n_bytes);
  if (written_bytes != n_bytes) {
    return -1;
  }

  if (log_fd >= 0) log(log_fd, &message);
  return 0;
}

int rpc_recv_req(const Connection* connection, RPCMessage* request) {
  if (-1 == readn(connection->sock_fd, &request->mark, sizeof(RPCMark))) {
    return -1;
  }
  if (request->mark.header_len != sizeof(RPCHeader)) {
    return -1;
  }
  if (-1 == readn(connection->sock_fd, &request->header, sizeof(RPCHeader))) {
    return -1;
  }
  request->body = (uint8_t*)malloc(request->mark.data_len);
  if (-1 == readn(connection->sock_fd, request->body, request->mark.data_len)) {
    return -1;
  }
  if (-1 == now_usec(&request->header.req_recv_time_us)) {
    return -1;
  }
  return 0;
}

int rpc_recv_resp(const Connection* connection, RPCMessage* response) {
  if (-1 == readn(connection->sock_fd, &response->mark, sizeof(RPCMark))) {
    return -1;
  }
  if (response->mark.header_len != sizeof(RPCHeader)) {
    return -1;
  }
  if (-1 == readn(connection->sock_fd, &response->header, sizeof(RPCHeader))) {
    return -1;
  }
  response->body = (uint8_t*)malloc(response->mark.data_len);
  if (-1 == readn(connection->sock_fd, response->body, response->mark.data_len)) {
    return -1;
  }
  if (-1 == now_usec(&response->header.res_recv_time_us)) {
    return -1;
  }
  return 0;
}

int now_usec(uint64_t* out) {
  struct timeval now;
  if (-1 == gettimeofday(&now, NULL)) return -1;
  *out = now.tv_sec * 1000 * 1000 + now.tv_usec;
  return 0;
}

