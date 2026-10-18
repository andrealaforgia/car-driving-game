#include "road_texture.h"

#define BLOTCH_CELL 8
#define BLOTCH_AMPLITUDE 8
#define GRAIN_AMPLITUDE 6

static unsigned next_random(unsigned* state) {
  *state = *state * 1664525u + 1013904223u;
  return *state >> 8;
}

static int centred_noise(unsigned* state, int amplitude) {
  return (int)(next_random(state) % (2 * amplitude + 1)) - amplitude;
}

static uint32_t grey_pixel(int value) {
  uint32_t v = (uint32_t)value;
  return 0xFF000000u | (v << 16) | (v << 8) | v;
}

/* Coarse blotches on an 8 pixel grid plus per-pixel grain; the two layers
 * together stay within ROAD_TEXTURE_MAX_DEVIATION of the base grey. */
void generate_asphalt_pixels(uint32_t* pixels, unsigned seed) {
  unsigned state = seed;
  const int cells = ROAD_TEXTURE_SIZE / BLOTCH_CELL;
  int blotches[ROAD_TEXTURE_SIZE / BLOTCH_CELL]
              [ROAD_TEXTURE_SIZE / BLOTCH_CELL];
  for (int cy = 0; cy < cells; ++cy) {
    for (int cx = 0; cx < cells; ++cx) {
      blotches[cy][cx] = centred_noise(&state, BLOTCH_AMPLITUDE);
    }
  }
  for (int y = 0; y < ROAD_TEXTURE_SIZE; ++y) {
    for (int x = 0; x < ROAD_TEXTURE_SIZE; ++x) {
      int value = ROAD_TEXTURE_BASE_GREY +
                  blotches[y / BLOTCH_CELL][x / BLOTCH_CELL] +
                  centred_noise(&state, GRAIN_AMPLITUDE);
      pixels[y * ROAD_TEXTURE_SIZE + x] = grey_pixel(value);
    }
  }
}
