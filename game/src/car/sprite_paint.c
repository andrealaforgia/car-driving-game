#include "sprite_paint.h"

#include <math.h>

double paint_clamp(double value) {
  return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

double paint_coverage(double distance) { return paint_clamp(0.5 - distance); }

double paint_grain(int x, int y) {
  unsigned n = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u;
  n = n * 1664525u + 1013904223u;
  return ((n >> 8) & 0xFFFF) / 65535.0 - 0.5;
}

rgb_t paint_shade(rgb_t colour, double luminance) {
  rgb_t shaded = {paint_clamp(colour.r * luminance),
                  paint_clamp(colour.g * luminance),
                  paint_clamp(colour.b * luminance)};
  return shaded;
}

rgb_t paint_mix(rgb_t from, rgb_t to, double amount) {
  rgb_t mixed = {from.r + (to.r - from.r) * amount,
                 from.g + (to.g - from.g) * amount,
                 from.b + (to.b - from.b) * amount};
  return mixed;
}

void paint_layer(layered_t* out, rgb_t colour, double cover) {
  out->r = out->r * (1.0 - cover) + colour.r * cover;
  out->g = out->g * (1.0 - cover) + colour.g * cover;
  out->b = out->b * (1.0 - cover) + colour.b * cover;
  out->a = out->a * (1.0 - cover) + cover;
}

uint32_t paint_pack(const layered_t* out) {
  if (out->a <= 0.0) {
    return 0u;
  }
  uint32_t a = (uint32_t)lround(paint_clamp(out->a) * 255.0);
  uint32_t r = (uint32_t)lround(paint_clamp(out->r / out->a) * 255.0);
  uint32_t g = (uint32_t)lround(paint_clamp(out->g / out->a) * 255.0);
  uint32_t b = (uint32_t)lround(paint_clamp(out->b / out->a) * 255.0);
  return (a << 24) | (b << 16) | (g << 8) | r;
}
