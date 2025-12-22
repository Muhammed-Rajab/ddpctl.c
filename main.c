#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "ddp.h"
#include "gifdec.h"

// dimension of the matrix
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

#define BYTES_PER_LED 3
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// gamma correction per channel
#define R_GAMMA 2.2f
#define G_GAMMA 2.0f
#define B_GAMMA 2.4f

// color correction per channel
#define R_CORRECTION 1.00f
#define G_CORRECTION 0.82f
#define B_CORRECTION 0.70f

// config
#define MIN_DELAY_IN_MS 16
typedef struct {
  const char *filename; // file path
  float brightness;     // [0.0, 1.0]
  int loop_count;       // -1 = infinite
} Config;

Config g_cfg = {.filename = NULL, .brightness = 0.5f, .loop_count = -1};

void parse_cli(int argc, char **argv) {
  int opt;

  while ((opt = getopt(argc, argv, "f:b:l:h")) != -1) {
    switch (opt) {

    case 'f':
      g_cfg.filename = optarg;
      break;

    case 'b': {
      char *end;
      errno = 0;
      g_cfg.brightness = strtof(optarg, &end);

      if (errno || end == optarg || g_cfg.brightness < 0.0f ||
          g_cfg.brightness > 1.0f) {
        fprintf(stderr, "invalid brightness: %s (0.0–1.0)\n", optarg);
        exit(1);
      }
      break;
    }

    case 'l': {
      char *end;
      errno = 0;
      long v = strtol(optarg, &end, 10);

      if (v < -1 || v > INT_MAX) {
        fprintf(stderr, "loop count out of range\n");
        exit(1);
      }

      if (errno || end == optarg) {
        fprintf(stderr, "invalid loop count: %s\n", optarg);
        exit(1);
      }

      g_cfg.loop_count = (int)v;

      if (g_cfg.loop_count == 0) {
        fprintf(stderr, "loop count 0: nothing to play\n");
        exit(1);
      }

      break;
    }

    case 'h':
    default:
      fprintf(stderr,
              "Usage: %s -f <gif> [-b <0-1>] [-l <loops>]\n"
              "  -f <gif>    GIF filename (required)\n"
              "  -b <0-1>    Brightness (default 0.5)\n"
              "  -l <n>      Loop count (-1 = infinite, default)\n",
              argv[0]);
      exit(0);
    }
  }

  if (!g_cfg.filename) {
    fprintf(stderr, "GIF filename required (-f)\n");
    exit(1);
  }
}

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
    if (!frame_buffer) {
      free(*frames);
      free(*delays_in_ms);
      gd_close_gif(handler);
      fprintf(stderr, "framebuffer allocation failed\n");
      return 0;
    }

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
      r = (uint8_t)(r * g_cfg.brightness);
      g = (uint8_t)(g * g_cfg.brightness);
      b = (uint8_t)(b * g_cfg.brightness);

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

int main(int argc, char **argv) {

  // parse cli
  parse_cli(argc, argv);

  // precalculate gamma values
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
      extract_gif_frames(g_cfg.filename, &frames, &delays_in_ms);
  if (frame_count == 0 || !frames || !delays_in_ms) {
    fprintf(stderr, "failed to extract frames\n");
    return 1;
  }

  size_t cur_frame_index = 0;
  int loops_done = 0;

  while (g_cfg.loop_count < 0 || loops_done < g_cfg.loop_count) {
    // update only data
    ddp.data = frames[cur_frame_index];

    size_t packet_size = 0;
    uint8_t *serialized_packet = DDP_serialize(&ddp, &packet_size);

    if (!serialized_packet || packet_size == 0) {
      fprintf(stderr, "failed to serialize ddp to packet\n");
      free_frames_and_delays(frames, delays_in_ms, frame_count);
      return 1;
    }

    // write frame to stdout immediately
    fwrite(serialized_packet, 1, packet_size, stdout);
    fflush(stdout);

    free(serialized_packet);

    usleep(delays_in_ms[cur_frame_index] * 1000);

    // move to next frame
    cur_frame_index += 1;

    // loop
    if (cur_frame_index == frame_count) {
      cur_frame_index = 0;
      loops_done += 1;
    }
  }

  // clean the other mess (maybe use valgrind?)
  free_frames_and_delays(frames, delays_in_ms, frame_count);
  return 0;
}
