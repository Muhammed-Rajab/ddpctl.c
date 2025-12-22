#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "ddp.h"
#include "gifdec.h"

// macros
#define CLAMP(x) ((x) < 0 ? 0 : (x) > 255 ? 255 : (x))

// dimension of the matrix
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

#define BYTES_PER_LED 3
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// led brightness [0.0, 1.0]
#define BRIGHTNESS 0.5

// gamma correction per channel
#define R_GAMMA 2.2f
#define G_GAMMA 2.0f
#define B_GAMMA 2.4f

// color correction per channel
#define R_CORRECTION 1.00f
#define G_CORRECTION 0.82f
#define B_CORRECTION 0.70f

// configs
#define LOOP_FOREVER
#define MIN_DELAY_IN_MS 16

// precalculate gamma values
uint8_t gamma_lut_r[256];
uint8_t gamma_lut_g[256];
uint8_t gamma_lut_b[256];

void init_gamma(void) {
  for (int i = 0; i < 256; i++) {
    float x = i / 255.0f;
    gamma_lut_r[i] = (uint8_t)(powf(x, R_GAMMA) * 255.0f + 0.5f);
    gamma_lut_g[i] = (uint8_t)(powf(x, G_GAMMA) * 255.0f + 0.5f);
    gamma_lut_b[i] = (uint8_t)(powf(x, B_GAMMA) * 255.0f + 0.5f);
  }
}

static inline uint8_t clamp_u8(int x) {
  if (x < 0)
    return 0;
  if (x > 255)
    return 255;
  return (uint8_t)x;
}

size_t extract_gif_frames(const char *fname, uint8_t ***frames,
                          size_t **delays_in_ms) {
  // returns frame count, sets array of delays and array of frames
  gd_GIF *handler = gd_open_gif(fname);
  if (!handler) {
    *frames = NULL;
    *delays_in_ms = NULL;
    fprintf(stderr, "failed to open gif: %s\n", fname);
    return 0;
  }

  if (handler->width != MATRIX_WIDTH || handler->height != MATRIX_HEIGHT) {
    *frames = NULL;
    *delays_in_ms = NULL;
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
  if (!*frames || !*delays_in_ms) {
    free(*frames);
    free(*delays_in_ms);
    gd_close_gif(handler);
    return 0;
  }

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
      r = clamp_u8((int)(r * R_CORRECTION));
      g = clamp_u8((int)(g * G_CORRECTION));
      b = clamp_u8((int)(b * B_CORRECTION));

      // brightness
      r = (uint8_t)(r * BRIGHTNESS);
      g = (uint8_t)(g * BRIGHTNESS);
      b = (uint8_t)(b * BRIGHTNESS);

      // gamma correction
      r = gamma_lut_r[clamp_u8(r)];
      g = gamma_lut_g[clamp_u8(g)];
      b = gamma_lut_b[clamp_u8(b)];

      // update frame buffer
      frame_buffer[i * 3 + 0] = r;
      frame_buffer[i * 3 + 1] = g;
      frame_buffer[i * 3 + 2] = b;
    }

    (*frames)[current_frame] = frame_buffer;
    (*delays_in_ms)[current_frame] =
        delay_in_ms <= 0 ? MIN_DELAY_IN_MS : delay_in_ms;
    current_frame += 1;
  }

  gd_close_gif(handler);

  return frame_count;
}

void free_frames_and_delays(uint8_t **frames, size_t *delays,
                            size_t frame_count) {
  if (!frames || !delays)
    return;

  for (size_t i = 0; i < frame_count; i++) {
    free(frames[i]);
  }
  free(frames);
  free(delays);
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

  size_t frame_count =
      extract_gif_frames("./resized.gif", &frames, &delays_in_ms);
  if (frame_count == 0 || !frames || !delays_in_ms) {
    fprintf(stderr, "failed to extract frames\n");
    return 1;
  }

  size_t cur_frame_index = 0;

  while (1) {
    // update only data
    ddp.data = frames[cur_frame_index];

    size_t packet_size = 0;
    uint8_t *serialized_packet = DDP_serialize(&ddp, &packet_size);

    if (!serialized_packet || packet_size == 0) {
      fprintf(stderr, "failed to serialize ddp to packet\n");
      return 1;
    }

    // write frame to stdout immediately
    fwrite(serialized_packet, 1, packet_size, stdout);
    fflush(stdout);

    free(serialized_packet);

    usleep(delays_in_ms[cur_frame_index] * 1000);

    // move to next frame
    cur_frame_index += 1;

#ifdef LOOP_FOREVER
    // loop
    if (cur_frame_index % frame_count == 0) {
      cur_frame_index = 0;
    }
#endif
  }

  // clean the other mess (maybe use valgrind?)
  free_frames_and_delays(frames, delays_in_ms, frame_count);
  return 0;
}
