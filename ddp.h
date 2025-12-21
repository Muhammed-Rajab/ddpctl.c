#ifndef DDP_H
#define DDP_H

#include <arpa/inet.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DDP_HEADER_SIZE 10

struct ddp_header {
  uint8_t flags;   // 0x41
  uint8_t res1;    // 0x0
  uint8_t type;    // 0x03 for RGB
  uint8_t res2;    // 0x0
  uint32_t offset; // 0x0
  uint16_t length; // no of data bytes
};

typedef uint8_t *ddp_data;

uint8_t *ddp_header_serialize(const struct ddp_header *header) {
  uint8_t *buf = (uint8_t *)malloc(DDP_HEADER_SIZE);

  if (!buf)
    return NULL;

  buf[0] = header->flags;
  buf[1] = header->res1;
  buf[2] = header->type;
  buf[3] = header->res2;

  uint32_t off = htonl(header->offset);
  uint16_t len = htons(header->length);

  memcpy(buf + 4, &off, 4);
  memcpy(buf + 8, &len, 2);

  return buf; // 10 byte
}

struct DDP {
  struct ddp_header header;
  ddp_data data;
};

uint8_t *DDP_serialize(const struct DDP *ddp, size_t *packet_size) {
  if (!ddp || !packet_size)
    return NULL;

  *packet_size = DDP_HEADER_SIZE + ddp->header.length;
  uint8_t *packet = (uint8_t *)malloc(*packet_size);
  if (!packet) {
    packet_size = 0;
    return NULL;
  }

  uint8_t *serialized_header = ddp_header_serialize(&ddp->header);
  if (!serialized_header) {
    free(packet);
    return NULL;
  }

  memcpy(packet, serialized_header, DDP_HEADER_SIZE);
  memcpy(packet + DDP_HEADER_SIZE, ddp->data, ddp->header.length);

  free(serialized_header);
  return packet;
}

void DDP_hexdump(const uint8_t *packet, const size_t packet_size) {
  for (size_t i = 0; i < packet_size; i++) {
    printf("%02X ", packet[i]);
  }
  printf("\n");
}

#endif // ! DDP_H
