#ifndef DDP_H
#define DDP_H

#include <stdint.h>
#include <stdlib.h>

// for all my needs, 10 bytes header is enough
#define DDP_HEADER_SIZE 10

// header structure of a DDP packet
struct ddp_header {
  uint8_t flags;   // 0x41
  uint8_t res1;    // 0x0
  uint8_t type;    // 0x03 for RGB
  uint8_t res2;    // 0x0
  uint32_t offset; // 0x0
  uint16_t length; // no of data bytes
};

typedef uint8_t *ddp_data;

uint8_t *ddp_header_serialize(const struct ddp_header *header);

// DDP packet
struct DDP {
  struct ddp_header header;
  ddp_data data;
};

uint8_t *DDP_serialize(const struct DDP *ddp, size_t *packet_size);

void DDP_hexdump(const uint8_t *packet, const size_t packet_size);

#endif // ! DDP_H
