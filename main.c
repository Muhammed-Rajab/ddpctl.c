#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ddp.h"

#define NUM_LEDS 256
#define BYTES_PER_LED 3

int main() {
  struct ddp_header header;
  header.flags = 0x41;
  header.res1 = 0x0;
  header.type = 0x03;
  header.res2 = 0x0;
  header.offset = 0x0;
  header.length = NUM_LEDS * BYTES_PER_LED;

  ddp_data data = (ddp_data)malloc(header.length);
  if (!data)
    return 1;

  for (int i = 0; i < NUM_LEDS; i++) {
    int y = i / 16;
    int x = i % 16;

    if (x == y || x + y == 16) {
      data[i * 3] = 255;
      data[i * 3 + 1] = 0;
      data[i * 3 + 2] = 0;
    }
  }

  struct DDP ddp;
  ddp.header = header;
  ddp.data = data;

  size_t packet_size = 0;
  uint8_t *serialized_packet = DDP_serialize(&ddp, &packet_size);

  if (!serialized_packet) {
    packet_size = 0;
    fprintf(stderr, "failed to serialize ddp to packet\n");
    return 0;
  }

  // DDP_hexdump(serialized_packet, packet_size);
  for (size_t i = 0; i < packet_size; i++) {
    printf("%c", serialized_packet[i]);
  }

  return 0;
}
