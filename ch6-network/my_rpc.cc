#include "my_rpc.h"

#include <arpa/inet.h>

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
