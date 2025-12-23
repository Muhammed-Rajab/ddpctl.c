# ddpctl

`ddpctl` is a small C program that turns GIF animations into DDP frames for LED matrices (like WLED).

It’s designed to be fast and simple. Instead of sending data over the network by itself, it just writes the DDP packets to `stdout`. This makes it easy to pipe the output into tools like `nc` or `socat` to send the frames over UDP.


## Features

- Decode GIF animations using `gifdec` (no ffmpeg dependency)
- Sample GIF frames to a fixed LED matrix grid
- Clean center-cell sampling (no blurry interpolation)
- Brightness control
- Per-frame delay handling with minimum delay parameter
- Optional gamma correction via lookup tables
- Streams raw DDP packets to `stdout`
- Transport-agnostic (works with any UDP client)
- ~45–60 FPS on 16×16 matrices


## Project Structure

```
.
├── ddpctl.c          # Entry point (main)
├── src/              # Application source files
│   ├── cli.c
│   ├── ddp.c
│   └── gif.c
├── include/          # Public headers
│   ├── cli.h
│   ├── config.h
│   ├── ddp.h
│   └── gif.h
├── lib/              # External dependencies
│   └── gifdec/       
│       ├── gifdec.c
│       └── gifdec.h
├── gifs/             # Test GIFs
├── build/            # Build artifacts
├── Makefile
└── README.md
````


## Build

Requirements:
- GCC or Clang
- POSIX-compatible system (Linux, macOS, BSD)

Build with:

```sh
make
````

Clean build artifacts:

```sh
make clean
```


## Usage

Basic usage:

```sh
./ddpctl -f gifs/eye.gif | nc -u <WLED_IP> 4048
```

### Options

* `-f <file>`
  Path to GIF file (required)

* `-b <value>`
  Brightness multiplier (0.0 – 1.0)
  Default: `0.5`

* `-l <count>`
  Loop count
  `-1` = infinite loop
  Default: `1`


## Design Notes

### Why stdout instead of built-in UDP?

`ddpctl` does **not** send UDP packets directly.

Instead, it writes raw DDP packets to `stdout`.
This allows users to choose how packets are transported:

```sh
# netcat
./ddpctl -f anim.gif | nc -u 192.168.1.50 4048

# socat
./ddpctl -f anim.gif | socat - UDP:192.168.1.50:4048
```

Advantages:

* Portable across systems
* No OS-specific networking code
* Composable with standard UNIX tools
* Easier debugging and testing


### GIF Sampling Strategy

* GIFs must be approximately square (aspect ratio check enforced)
* The image is divided into a fixed grid matching the LED matrix
* Each LED samples the **center pixel** of its corresponding cell

This avoids:

* Color bleeding
* Blurred edges
* Interpolation artifacts


### Color Processing Pipeline

For each sampled pixel:

1. Channel correction (optional, configurable)
2. Brightness scaling
3. Gamma correction using precomputed LUTs

Note:
Depending on your LED strip, channel correction may not be necessary.
Some strips are already factory-balanced.


### Performance

* Frame decoding and sampling are done once per frame
* Gamma correction uses lookup tables (no per-pixel `powf`)
* Typical performance: **45–60 FPS** on 16×16 matrices


## Notes

* This project intentionally avoids ffmpeg
* Only GIF version 89a is supported due to `gifdec`
* Designed as a small, focused tool — not a full animation engine


## License

This project is licensed under the MIT License. See the LICENSE file for details.
