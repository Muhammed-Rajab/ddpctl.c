#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "include/cli.h"
#include "include/ddp.h"
#include "include/gif.h"

#include "include/config.h"

Config g_cfg = {.filename = NULL, .brightness = 0.5f, .loop_count = -1};

int main(int argc, char **argv) {

  // parse cli
  parse_cli(argc, argv, &g_cfg);

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

  size_t frame_count = extract_gif_frames(g_cfg.filename, &frames,
                                          &delays_in_ms, g_cfg.brightness);
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
