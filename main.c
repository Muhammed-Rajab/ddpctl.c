#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ddp.h"
#include "gifdec.h"

#define NUM_LEDS 256
#define BYTES_PER_LED 3

#define CLAMP(x) ((x) > 255 ? 255 : (x))

uint8_t gamma_lut[256];

uint8_t gamma_r[256];
uint8_t gamma_g[256];
uint8_t gamma_b[256];

void init_gamma(void) {
  for (int i = 0; i < 256; i++) {
    float x = i / 255.0f;
    gamma_r[i] = (uint8_t)(powf(x, 2.2f) * 255.0f + 0.5f);
    gamma_g[i] = (uint8_t)(powf(x, 2.0f) * 255.0f + 0.5f);
    gamma_b[i] = (uint8_t)(powf(x, 2.4f) * 255.0f + 0.5f);
  }
}

uint8_t *compose_ddp_packet(uint8_t *frame_buffer, size_t *packet_size) {
  if (!frame_buffer || !packet_size) {
    return NULL;
  }

  // PACKET SETUP
  struct ddp_header header;
  header.flags = 0x41;
  header.res1 = 0x0;
  header.type = 0x03;
  header.res2 = 0x0;
  header.offset = 0x0;
  header.length = NUM_LEDS * BYTES_PER_LED;

  struct DDP ddp;
  ddp.header = header;
  ddp.data = frame_buffer;

  size_t tmp_packet_size = 0;
  uint8_t *serialized_packet = DDP_serialize(&ddp, &tmp_packet_size);

  if (!serialized_packet) {
    tmp_packet_size = 0;
    fprintf(stderr, "failed to serialize ddp to packet\n");
    return 0;
  }

  *packet_size = tmp_packet_size;

  return serialized_packet;
}

int main() {

  init_gamma();

  gd_GIF *handler = gd_open_gif("./michel.gif");
  if (!handler) {
    fprintf(stderr, "failed to open gif\n");
    return 1;
  }

  // WARN: now only allow 16x16 gifs
  if (handler->width != 16 || handler->height != 16) {
    fprintf(stderr, "can't accept non 16x16 gifs for now\n");
    return 1;
  }

  uint8_t *frame_buffer =
      (uint8_t *)malloc(handler->width * handler->height * 3);

  if (!frame_buffer) {
    fprintf(stderr, "failed to allocated frame buffer\n");
    return 1;
  }

  while (1) {
    while (gd_get_frame(handler) != 0) {

      int delay_in_ms = handler->gce.delay * 10;

      gd_render_frame(handler, frame_buffer);

      // TODO: gamma correction
      for (int i = 0; i < NUM_LEDS; i++) {

        uint8_t r = frame_buffer[i * 3 + 0];
        uint8_t g = frame_buffer[i * 3 + 1];
        uint8_t b = frame_buffer[i * 3 + 2];

        // Color correction (calibration)
        r = CLAMP((int)(r * 1.00f));
        g = CLAMP((int)(g * 0.82f));
        b = CLAMP((int)(b * 0.70f));

        // Brightness (linear)
        float br = .5f;
        r = (uint8_t)(r * br);
        g = (uint8_t)(g * br);
        b = (uint8_t)(b * br);

        r = gamma_r[r];
        g = gamma_g[g];
        b = gamma_b[b];

        frame_buffer[i * 3 + 0] = r;
        frame_buffer[i * 3 + 1] = g;
        frame_buffer[i * 3 + 2] = b;
      }

      size_t packet_size = 0;
      uint8_t *serialized_packet =
          compose_ddp_packet(frame_buffer, &packet_size);

      fwrite(serialized_packet, 1, packet_size, stdout);
      fflush(stdout);

      free(serialized_packet);

      usleep(delay_in_ms * 1000);
    }

    gd_rewind(handler);
  }

  free(frame_buffer);
  gd_close_gif(handler);

  return 0;
}
