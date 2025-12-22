#ifndef CONFIG_H
#define CONFIG_H

// dimension of the matrix
#define MATRIX_WIDTH 16
#define MATRIX_HEIGHT 16

// WARN: don't change these values
#define BYTES_PER_LED 3
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// gamma correction per channel. tune these values according to
// your LED's response.
#define R_GAMMA 2.2f
#define G_GAMMA 2.0f
#define B_GAMMA 2.4f

// color correction per channel (for me 1.00f is good enough)
#define R_CORRECTION 1.00f
#define G_CORRECTION 1.00f
#define B_CORRECTION 1.00f

// config
#define MIN_DELAY_IN_MS 16

#endif // CONFIG_H
