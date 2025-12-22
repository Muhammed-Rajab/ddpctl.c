#include "../include/gif.h"

#include <math.h>
#include <stdio.h>

#include "../include/config.h"
#include "../lib/gifdec/gifdec.h"

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

size_t extract_gif_frames(const char *fname, uint8_t ***frames,
                          size_t **delays_in_ms, float br) {
  // returns frame count, sets array of delays and array of frames
  gd_GIF *handler = gd_open_gif(fname);
  if (!handler) {
    *frames = NULL;
    *delays_in_ms = NULL;
    fprintf(stderr, "failed to open gif: %s\n", fname);
    return 0;
  }

  float ar = (float)handler->width / (float)handler->height;

  if (fabsf(ar - 1.0f) > 0.02f) {
    *frames = NULL;
    *delays_in_ms = NULL;
    gd_close_gif(handler);
    fprintf(stderr, "the gif is not square enough (%.3f)\n", ar);
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

  int cell_w = handler->width / MATRIX_WIDTH;
  int cell_h = handler->height / MATRIX_HEIGHT;

  size_t current_frame = 0;
  while (gd_get_frame(handler) != 0) {

    int delay_in_ms = handler->gce.delay * 10;

    uint8_t *frame_buffer =
        (uint8_t *)malloc(handler->width * handler->height * BYTES_PER_LED);

    uint8_t *data_buffer = (uint8_t *)malloc(NUM_LEDS * BYTES_PER_LED);

    if (!frame_buffer || !data_buffer) {
      free(*frames);
      free(*delays_in_ms);
      gd_close_gif(handler);
      fprintf(stderr, "framebuffer or databuffer allocation failed\n");
      return 0;
    }

    gd_render_frame(handler, frame_buffer);

    for (int y = 0; y < MATRIX_HEIGHT; y += 1) {
      for (int x = 0; x < MATRIX_WIDTH; x += 1) {
        int px = x * cell_w + cell_w / 2;
        int py = y * cell_h + cell_h / 2;
        size_t index = py * handler->width + px;

        uint8_t r = frame_buffer[index * 3 + 0];
        uint8_t g = frame_buffer[index * 3 + 1];
        uint8_t b = frame_buffer[index * 3 + 2];

        // color correction
        r = clamp_u8((int)(r * R_CORRECTION));
        g = clamp_u8((int)(g * G_CORRECTION));
        b = clamp_u8((int)(b * B_CORRECTION));

        // brightness
        r = (uint8_t)(r * br);
        g = (uint8_t)(g * br);
        b = (uint8_t)(b * br);

        // gamma correction
        r = gamma_lut_r[clamp_u8(r)];
        g = gamma_lut_g[clamp_u8(g)];
        b = gamma_lut_b[clamp_u8(b)];

        // update frame buffer
        size_t data_index = y * MATRIX_WIDTH + x;
        data_buffer[data_index * 3 + 0] = r;
        data_buffer[data_index * 3 + 1] = g;
        data_buffer[data_index * 3 + 2] = b;
      }
    }

    // for (int i = 0; i < NUM_LEDS; i++) {
    //
    //   uint8_t r = frame_buffer[i * 3 + 0];
    //   uint8_t g = frame_buffer[i * 3 + 1];
    //   uint8_t b = frame_buffer[i * 3 + 2];
    //
    //   // color correction
    //   r = clamp_u8((int)(r * R_CORRECTION));
    //   g = clamp_u8((int)(g * G_CORRECTION));
    //   b = clamp_u8((int)(b * B_CORRECTION));
    //
    //   // brightness
    //   r = (uint8_t)(r * br);
    //   g = (uint8_t)(g * br);
    //   b = (uint8_t)(b * br);
    //
    //   // gamma correction
    //   r = gamma_lut_r[clamp_u8(r)];
    //   g = gamma_lut_g[clamp_u8(g)];
    //   b = gamma_lut_b[clamp_u8(b)];
    //
    //   // update frame buffer
    //   frame_buffer[i * 3 + 0] = r;
    //   frame_buffer[i * 3 + 1] = g;
    //   frame_buffer[i * 3 + 2] = b;
    // }
    //
    (*frames)[current_frame] = data_buffer;
    (*delays_in_ms)[current_frame] =
        delay_in_ms <= 0 ? MIN_DELAY_IN_MS : delay_in_ms;
    current_frame += 1;
    free(frame_buffer);
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
