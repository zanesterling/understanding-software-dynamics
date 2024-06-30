#pragma once

#include <stdint.h>

#include "assert.h"

// An RPC request or response message starts with an RPC marker followed by an
// RPC header followed optionally by a byte string that contains the argument
// values for a request or the result values for a response.
//
// +------+------------+----------
// | Mark | RPC header | data ...
// +------+------------+----------
//   16 B      72 B       0..N B
//
// The 16 B RPC marker, shown below, serves several purposes: delimiting
// messages, defining variable lengths, and sanity check.
//
// +-----------+-----------+---------+----------+
// | signature | headerlen | datalen | checksum |
// +-----------+-----------+---------+----------+

constexpr uint32_t MARK_SIGNATURE = 0xdeadbeef;

struct RPCMark {
  uint32_t signature;
  uint32_t header_len;
  uint32_t data_len;
  uint32_t checksum;

  void pretty_print();
};

static_assert(sizeof(RPCMark) == 16);

const uint16_t TYPE_REQUEST  = 0;
const uint16_t TYPE_RESPONSE = 1;

struct RPCHeader {
  // Unique ID number for each outstanding request.
  uint32_t rpc_id;

  // RPC ID of the request that spawned this request.
  // 0 if not spawned by another request.
  uint32_t parent;

  // Wall-clock timestamps T1,..,T4 of {request,response} {send,receive}.
  // Given in microseconds.
  uint64_t req_send_time_us;
  uint64_t req_recv_time_us;
  uint64_t res_send_time_us;
  uint64_t res_recv_time_us;

  uint32_t client_ip;
  uint32_t server_ip;
  uint16_t client_port;
  uint16_t server_port;

  // Base-2 logarithm of the byte length of the {request,response}.
  uint8_t req_len_log;
  uint8_t res_len_log;

  // Request, response, or some other kind of message.
  uint16_t message_type;

  // ASCII name of the routine being called, zero-padded.
  char method[8];

  // Return-value status indicating success, failure, or specific error number.
  uint32_t status;

  // uint32_t pad;

  void pretty_print();
};

static_assert(sizeof(RPCHeader) == 72);

struct RPCMessage {
  RPCMark mark;
  RPCHeader header;

  // Returns a pointer to the first byte after `header`, where the body of the
  // request / response is stored.
  uint8_t* data();

  int send(int sock_fd);
};

static_assert(sizeof(RPCMessage) == sizeof(RPCMark) + sizeof(RPCHeader));

