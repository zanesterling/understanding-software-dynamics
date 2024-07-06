#include "my_rpc.h"

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

WriteRequest* WriteRequest::Make(
  const char* const key,
  const uint8_t key_len,
  const char* const value,
  const uint32_t value_len
) {
  if (value_len > 5 * 256 * 1024) return NULL;
  size_t buf_size = sizeof(WriteRequest) + key_len + value_len;
  void* buf = aligned_alloc(sizeof(WriteRequest), buf_size);

  WriteRequest* request = (WriteRequest*) buf;
  request->key_len_   = key_len;
  request->value_len_ = htonl(value_len);
  memcpy(request->key(), key, key_len);
  memcpy(request->value(), value, value_len);
  return request;
}

WriteRequest* WriteRequest::FromBody(void* rpc_body, size_t body_len) {
  WriteRequest* request = (WriteRequest*) rpc_body;
  size_t request_len = sizeof(WriteRequest) + request->key_len() + request->value_len();
  if (request_len != body_len) return NULL;

  return request;
}

char* WriteRequest::key() {
  return (char*) (this+1);
}

char* WriteRequest::value() {
  return this->key_len() + (char*) (this+1);
}

size_t WriteRequest::key_len() {
  return this->key_len_;
}

size_t WriteRequest::value_len() {
  return ntohl(this->value_len_);
}

size_t WriteRequest::full_len() {
  return sizeof(*this) + this->key_len() + this->value_len();
}

