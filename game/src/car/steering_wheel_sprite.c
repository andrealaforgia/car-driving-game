#include "steering_wheel_sprite.h"

#include <math.h>

#include "geometry.h"
#include "sprite_paint.h"

/* Radii as fractions of half the sprite size. */
#define RIM_RADIUS 0.90
#define RIM_HALF_THICKNESS 0.085
#define STITCH_DEPTH -0.5
#define STITCH_WIDTH 0.1
#define STITCH_PITCH_FRACTION 0.022
#define SPOKE_HALF_WIDTH 0.075
#define SPOKE_TAPER 0.3
#define HUB_RADIUS 0.27
#define HUB_BEVEL 0.045
#define PAD_RADIUS 0.19
#define EMBLEM_RADIUS 0.06
#define EMBLEM_THICKNESS 0.012
#define SPOKES 3

static const rgb_t LEATHER = {0.20, 0.18, 0.17};
static const rgb_t THREAD = {0.66, 0.58, 0.46};
static const rgb_t METAL = {0.60, 0.61, 0.65};
static const rgb_t PAD = {0.24, 0.23, 0.23};
static const rgb_t EMBLEM = {0.78, 0.78, 0.82};
static const double LIGHT[3] = {-0.45, -0.60, 0.66};
static const double SPOKE_ANGLES[SPOKES] = {M_PI, 0.0, M_PI / 2.0};

/* The rim is a leather tube: shaded as a torus lit from the top left, with
 * a soft sheen fading towards the bottom right and a row of stitches along
 * its inner edge. */
static void paint_rim(layered_t* out, double px, double py, double half, int x,
                      int y) {
  double r = hypot(px, py);
  double rim_radius = RIM_RADIUS * half;
  double thickness = RIM_HALF_THICKNESS * half;
  double cover = paint_coverage(fabs(r - rim_radius) - thickness);
  if (cover <= 0.0) {
    return;
  }
  double n = paint_clamp(fabs(r - rim_radius) / thickness) *
             (r < rim_radius ? -1.0 : 1.0);
  double height = sqrt(1.0 - n * n);
  double normal[3] = {n * px / r, n * py / r, height};
  double diffuse =
      normal[0] * LIGHT[0] + normal[1] * LIGHT[1] + normal[2] * LIGHT[2];
  diffuse = diffuse < 0.0 ? 0.0 : diffuse;
  double sheen = 0.9 - 0.25 * (px + py) / (r * sqrt(2.0));
  double luminance = (0.30 + 0.75 * diffuse + 0.45 * pow(diffuse, 12.0)) *
                     sheen * (1.0 + 0.10 * paint_grain(x, y));
  rgb_t colour = paint_shade(LEATHER, luminance);
  double along = atan2(py, px) * rim_radius;
  double pitch = STITCH_PITCH_FRACTION * half;
  double phase = along - floor(along / pitch) * pitch;
  if (fabs(n - STITCH_DEPTH) < STITCH_WIDTH && phase < pitch * 0.55) {
    colour = paint_mix(colour, paint_shade(THREAD, 0.6 + 0.6 * diffuse), 0.75);
  }
  paint_layer(out, colour, cover);
}

/* Three tapered metal spokes from the hub to the rim, bevelled so the
 * centre line catches the light, with a dark edge. */
static void paint_spokes(layered_t* out, double px, double py, double half,
                         int x, int y) {
  (void)y;
  double rim_radius = RIM_RADIUS * half;
  for (int i = 0; i < SPOKES; ++i) {
    double ux = cos(SPOKE_ANGLES[i]);
    double uy = sin(SPOKE_ANGLES[i]);
    double along = px * ux + py * uy;
    double across = fabs(-px * uy + py * ux);
    double width = SPOKE_HALF_WIDTH * half *
                   (1.0 + SPOKE_TAPER * (0.5 - along / rim_radius));
    double distance = across - width;
    distance = fmax(distance, -along);
    distance = fmax(distance, along - rim_radius);
    double cover = paint_coverage(distance);
    if (cover <= 0.0) {
      continue;
    }
    double bevel = 1.0 - pow(paint_clamp(across / width), 2.0);
    double edge = across > width - 1.5 ? 0.55 : 1.0;
    double luminance = (0.5 + 0.55 * bevel) * edge *
                       (1.0 + 0.06 * paint_grain((int)along, i)) *
                       (1.0 - 0.15 * along / rim_radius);
    (void)x;
    paint_layer(out, paint_shade(METAL, luminance), cover);
  }
}

/* A padded hub with a bevelled edge and a small ring emblem. */
static void paint_hub(layered_t* out, double px, double py, double half, int x,
                      int y) {
  double r = hypot(px, py);
  double hub = HUB_RADIUS * half;
  double cover = paint_coverage(r - hub);
  if (cover <= 0.0) {
    return;
  }
  double up = r > 0.0 ? -py / r : 0.0;
  double luminance = 0.85 + 0.25 * up;
  if (r > hub - HUB_BEVEL * half) {
    double depth = (r - (hub - HUB_BEVEL * half)) / (HUB_BEVEL * half);
    luminance = (1.1 + 0.8 * up) * (1.0 - 0.5 * depth * depth);
  }
  rgb_t colour =
      paint_shade(LEATHER, luminance * (1.0 + 0.08 * paint_grain(x, y)));
  double pad = PAD_RADIUS * half;
  double pad_cover = paint_coverage(r - pad);
  if (pad_cover > 0.0) {
    double dome = sqrt(1.0 - pow(paint_clamp(r / pad), 2.0));
    rgb_t padded = paint_shade(PAD, 0.8 + 0.35 * up + 0.3 * dome);
    colour = paint_mix(colour, padded, pad_cover);
  }
  double emblem = EMBLEM_RADIUS * half;
  double ring = paint_coverage(fabs(r - emblem) - EMBLEM_THICKNESS * half);
  if (ring > 0.0) {
    colour = paint_mix(colour, paint_shade(EMBLEM, 0.85 + 0.35 * up), ring);
  }
  paint_layer(out, colour, cover);
}

void render_steering_wheel_pixels(uint32_t* pixels, int size) {
  double half = size / 2.0;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      double px = x + 0.5 - half;
      double py = y + 0.5 - half;
      layered_t out = {0.0, 0.0, 0.0, 0.0};
      paint_rim(&out, px, py, half, x, y);
      paint_spokes(&out, px, py, half, x, y);
      paint_hub(&out, px, py, half, x, y);
      pixels[y * size + x] = paint_pack(&out);
    }
  }
}
