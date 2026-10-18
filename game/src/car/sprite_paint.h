/**
 * @file sprite_paint.h
 * @brief Helpers for painting anti-aliased, shaded sprites into RGBA
 * pixel buffers from signed distances: colours, coverage, layering with
 * premultiplied alpha, grain, and packing to RGBA32. Pure: no SDL.
 */

#ifndef GAME_SRC_CAR_SPRITE_PAINT_H_
#define GAME_SRC_CAR_SPRITE_PAINT_H_

#include <stdint.h>

typedef struct {
  double r, g, b;
} rgb_t;

typedef struct {
  double r, g, b, a; /* premultiplied */
} layered_t;

double paint_clamp(double value);
/* Fraction of a pixel inside a shape, from its signed distance in pixels
 * (negative inside). */
double paint_coverage(double distance);
/* Repeatable noise in -0.5..0.5 for a pixel. */
double paint_grain(int x, int y);
rgb_t paint_shade(rgb_t colour, double luminance);
rgb_t paint_mix(rgb_t from, rgb_t to, double amount);
/* Lays a colour over what is already there, covering the given fraction. */
void paint_layer(layered_t* out, rgb_t colour, double cover);
/* RGBA32: R in the lowest byte, A in the highest. */
uint32_t paint_pack(const layered_t* out);

#endif  // GAME_SRC_CAR_SPRITE_PAINT_H_
