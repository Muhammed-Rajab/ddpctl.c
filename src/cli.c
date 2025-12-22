#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/cli.h"

void parse_cli(int argc, char **argv, Config *cfg) {
  int opt;

  while ((opt = getopt(argc, argv, "f:b:l:h")) != -1) {
    switch (opt) {

    case 'f':
      cfg->filename = optarg;
      break;

    case 'b': {
      char *end;
      errno = 0;
      cfg->brightness = strtof(optarg, &end);

      if (errno || end == optarg || cfg->brightness < 0.0f ||
          cfg->brightness > 1.0f) {
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

      cfg->loop_count = (int)v;

      if (cfg->loop_count == 0) {
        fprintf(stderr, "loop count 0: nothing to play\n");
        exit(1);
      }

      break;
    }

    case 'h':
    default:
      fprintf(stderr,
              "usage: %s -f <gif> [-b <0-1>] [-l <loops>]\n"
              "  -f <gif>    GIF filename (required)\n"
              "  -b <0-1>    brightness (default 0.5)\n"
              "  -l <n>      loop count (-1 = infinite, default)\n",
              argv[0]);
      exit(0);
    }
  }

  if (!cfg->filename) {
    fprintf(stderr, "GIF filename required (-f)\n");
    exit(1);
  }
}
