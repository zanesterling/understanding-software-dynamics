// Defines the RPC interface for the server in ch 6.

#include <stddef.h>
#include <stdint.h>

// A string with a length.
// The string need not be null-terminated.
// Does not own its data pointer.
struct LenStr {
  size_t length;
  char* data;

  LenStr(size_t length, char* data) : length(length), data(data) {}
};

// Stored in the RPCMessage::body.
struct WriteRequest {
  uint8_t key_len;
  uint32_t value_len;

  // Parses an RPC body into a WriteRequest, returning a non-owning
  // pointer to the request.
  //
  // Returns NULL if parsing fails --
  // ie. sizeof(WriteRequest) + key_len + value_len != body_len.
  static WriteRequest* FromBody(void* rpc_body, size_t body_len);

  // Returns non-owning, non-null-terminated pointers to the key and
  // value, respectively.
  char* key();
  char* value();
};

