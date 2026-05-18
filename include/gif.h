#ifndef GIF_H
#define GIF_H

#include <stdint.h>
#include <stdlib.h>

// precalculate gamma values for each channel
void init_gamma(void);

static inline uint8_t clamp_u8(int x) {
  if (x < 0)
    return 0;
  if (x > 255)
    return 255;
  return (uint8_t)x;
}

size_t extract_gif_frames(const char *fname, uint8_t ***frames,
                          size_t **delays_in_ms, float br);

void free_frames_and_delays(uint8_t **frames, size_t *delays,
                            size_t frame_count);
#endif // GIF_H
