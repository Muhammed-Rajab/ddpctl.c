#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "ddp.h"
#include "gifdec.h"

#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)
#define BYTES_PER_LED 3

#define BRIGHTNESS 0.5

#define CLAMP(x) ((x) < 0 ? 0 : (x) > 255 ? 255 : (x))

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

size_t extract_frames(const char *fname, uint8_t ***frames,
                      size_t **delays_in_ms) {
  // returns frame count, sets array of delays and array of frames
  gd_GIF *handler = gd_open_gif(fname);
  if (!handler) {
    frames = NULL;
    delays_in_ms = NULL;
    fprintf(stderr, "failed to open gif: %s\n", fname);
    return 0;
  }

  if (handler->width != MATRIX_WIDTH || handler->height != MATRIX_HEIGHT) {
    frames = NULL;
    delays_in_ms = NULL;
    gd_close_gif(handler);
    fprintf(stderr, "can't accept non %dx%d for now\n", MATRIX_WIDTH,
            MATRIX_HEIGHT);
    return 0;
  }

  // extract frame count
  size_t frame_count = 0;
  while (gd_get_frame(handler) != 0) {
    frame_count += 1;
  }
  gd_rewind(handler);

  // allocate array for frames and delays
  *frames = (uint8_t **)malloc(frame_count * sizeof(uint8_t *));
  *delays_in_ms = (size_t *)malloc(frame_count * sizeof(size_t));

  size_t current_frame = 0;
  while (gd_get_frame(handler) != 0) {

    int delay_in_ms = handler->gce.delay * 10;

    uint8_t *frame_buffer = (uint8_t *)malloc(NUM_LEDS * BYTES_PER_LED);

    gd_render_frame(handler, frame_buffer);

    for (int i = 0; i < NUM_LEDS; i++) {

      uint8_t r = frame_buffer[i * 3 + 0];
      uint8_t g = frame_buffer[i * 3 + 1];
      uint8_t b = frame_buffer[i * 3 + 2];

      // color correction
      r = CLAMP((int)(r * 1.00f));
      g = CLAMP((int)(g * 0.82f));
      b = CLAMP((int)(b * 0.70f));

      // brightness
      r = (uint8_t)(r * BRIGHTNESS);
      g = (uint8_t)(g * BRIGHTNESS);
      b = (uint8_t)(b * BRIGHTNESS);

      // gamma correction
      r = gamma_r[r];
      g = gamma_g[g];
      b = gamma_b[b];

      frame_buffer[i * 3 + 0] = r;
      frame_buffer[i * 3 + 1] = g;
      frame_buffer[i * 3 + 2] = b;
    }

    (*frames)[current_frame] = frame_buffer;
    (*delays_in_ms)[current_frame] = delay_in_ms;
    current_frame += 1;
  }

  gd_close_gif(handler);

  return frame_count;
}

int main() {

  init_gamma();

  // create ddp header
  struct ddp_header header;
  header.flags = 0x41;
  header.res1 = 0x0;
  header.type = 0x03;
  header.res2 = 0x0;
  header.offset = 0x0;
  header.length = NUM_LEDS * BYTES_PER_LED;

  // only set header as it can be reused
  struct DDP ddp;
  ddp.header = header;

  // load gif frames and delays in between
  uint8_t **frames = NULL;
  size_t *delays_in_ms = NULL;

  size_t frame_count = extract_frames("./resized.gif", &frames, &delays_in_ms);
  if (frame_count == 0 || !frames || !delays_in_ms) {
    fprintf(stderr, "failed to extract frames\n");
    return 1;
  }

  size_t cur_frame_index = 0;
  while (1) {

    uint8_t *cur_frame = frames[cur_frame_index];
    size_t cur_frame_delay_in_ms = delays_in_ms[cur_frame_index];

    // update only data
    ddp.data = cur_frame;

    size_t packet_size = 0;
    uint8_t *serialized_packet = DDP_serialize(&ddp, &packet_size);

    if (!serialized_packet) {
      packet_size = 0;
      fprintf(stderr, "failed to serialize ddp to packet\n");
      return 1;
    }

    fwrite(serialized_packet, 1, packet_size, stdout);
    fflush(stdout);

    free(serialized_packet);

    usleep(cur_frame_delay_in_ms * 1000);

    cur_frame_index += 1;

    if (cur_frame_index % frame_count == 0) {
      cur_frame_index = 0;
    }
  }

  // clean the other mess (maybe use valgrind?)

  return 0;
}
