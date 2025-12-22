#ifndef CLI_H
#define CLI_H

typedef struct {
  const char *filename; // file path
  float brightness;     // [0.0, 1.0]
  int loop_count;       // -1 = infinite
} Config;

void parse_cli(int argc, char **argv, Config *cfg);

#endif // CLI_H
