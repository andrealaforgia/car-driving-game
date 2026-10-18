#include "dial_sprite.h"

#include <math.h>

#include "dashboard.h"
#include "geometry.h"
#include "sprite_paint.h"

#define BEZEL_WIDTH 0.08 /* of the radius */
#define MAJOR_LENGTH 0.14
#define MINOR_LENGTH 0.08
#define MAJOR_HALF_WIDTH 0.014
#define MINOR_HALF_WIDTH 0.008
#define RED_ZONE_HALF_WIDTH 0.035

static const rgb_t CHROME = {0.82, 0.84, 0.90};
static const rgb_t FACE = {0.075, 0.075, 0.085};
static const rgb_t PRINT = {0.94, 0.94, 0.92};
static const rgb_t RED = {0.85, 0.12, 0.10};
static const rgb_t GLASS = {1.0, 1.0, 1.0};
static const rgb_t NEEDLE = {0.95, 0.22, 0.14};
static const rgb_t WEIGHT = {0.12, 0.12, 0.13};

/* Signed distance to a line segment thickened into a capsule. */
static double capsule(double px, double py, double ax, double ay, double bx,
                      double by, double radius) {
  double abx = bx - ax, aby = by - ay;
  double t = ((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby);
  t = paint_clamp(t);
  return hypot(px - (ax + abx * t), py - (ay + aby * t)) - radius;
}

static double tick_distance(double px, double py, double radius, int majors,
                            bool* major_out, double* fraction_out) {
  int ticks = majors * 2;
  double best = 1e9;
  for (int tick = 0; tick <= ticks; ++tick) {
    bool major = tick % 2 == 0;
    double fraction = (double)tick / ticks;
    double angle = dial_angle(fraction);
    double outer = DIAL_TICK_RADIUS * radius;
    double inner = outer - (major ? MAJOR_LENGTH : MINOR_LENGTH) * radius;
    double half = (major ? MAJOR_HALF_WIDTH : MINOR_HALF_WIDTH) * radius;
    double d = capsule(px, py, inner * cos(angle), inner * sin(angle),
                       outer * cos(angle), outer * sin(angle), half);
    if (d < best) {
      best = d;
      *major_out = major;
      *fraction_out = fraction;
    }
  }
  return best;
}

/* Where a point lies along the sweep, or a negative number off it. */
static double sweep_fraction(double px, double py) {
  double angle = atan2(py, px);
  double from_start = angle - dial_angle(0.0);
  while (from_start < 0.0) {
    from_start += 2.0 * M_PI;
  }
  return from_start <= DIAL_SWEEP ? from_start / DIAL_SWEEP : -1.0;
}

void render_dial_face_pixels(uint32_t* pixels, int size, int majors,
                             double red_zone_from) {
  double radius = size / 2.0 - 1.0;
  double bezel = BEZEL_WIDTH * radius;
  double face_radius = radius - bezel;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      double px = x + 0.5 - size / 2.0;
      double py = y + 0.5 - size / 2.0;
      double r = hypot(px, py);
      layered_t out = {0.0, 0.0, 0.0, 0.0};
      double cover = paint_coverage(r - radius);
      if (cover > 0.0) {
        rgb_t colour;
        if (r > face_radius - 0.5) {
          /* Chrome bezel: a rounded ring lit from the top left. */
          double n =
              paint_clamp((r - (face_radius + bezel / 2.0)) / (bezel / 2.0) *
                          (r > face_radius + bezel / 2.0 ? 1.0 : -1.0));
          double height = sqrt(1.0 - n * n);
          double nx = n * px / r, ny = n * py / r;
          double diffuse = fmax(0.0, -0.45 * nx - 0.6 * ny + 0.66 * height);
          double luminance = 0.35 + 0.55 * diffuse + 0.5 * pow(diffuse, 10.0);
          colour = paint_shade(CHROME, luminance);
        } else {
          /* Matt black face, a touch lighter in the middle. */
          double shade = 1.25 - 0.45 * pow(r / face_radius, 2.0);
          colour = paint_shade(FACE, shade * (1.0 + 0.06 * paint_grain(x, y)));
          bool major = false;
          double at = 0.0;
          double tick = tick_distance(px, py, radius, majors, &major, &at);
          double fraction = sweep_fraction(px, py);
          bool hot = fraction >= red_zone_from;
          if (fraction >= 0.0 && fraction >= red_zone_from &&
              fabs(r - DIAL_RED_ZONE_RADIUS * radius) <
                  RED_ZONE_HALF_WIDTH * radius) {
            double band =
                paint_coverage(fabs(r - DIAL_RED_ZONE_RADIUS * radius) -
                               RED_ZONE_HALF_WIDTH * radius);
            colour = paint_mix(colour, RED, band);
          }
          double ink = paint_coverage(tick);
          if (ink > 0.0) {
            colour = paint_mix(colour, at >= red_zone_from || hot ? RED : PRINT,
                               ink);
          }
          /* Glass: a soft reflection across the upper left. */
          double gx = (px + 0.35 * face_radius) / (0.9 * face_radius);
          double gy = (py + 0.45 * face_radius) / (0.55 * face_radius);
          double glass = 1.0 - hypot(gx, gy);
          if (glass > 0.0) {
            colour = paint_mix(colour, GLASS, 0.10 * glass);
          }
        }
        paint_layer(&out, colour, cover);
      }
      pixels[y * size + x] = paint_pack(&out);
    }
  }
}

void render_needle_pixels(uint32_t* pixels, int width, int height) {
  double cx = width / 2.0;
  double pivot_y = NEEDLE_PIVOT_Y * height;
  double tip_y = 0.03 * height;
  double tail_y = 0.97 * height;
  double tip_half = 0.04 * width;
  double root_half = 0.17 * width;
  double tail_half = 0.15 * width;
  double hub = 0.42 * width;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double px = x + 0.5, py = y + 0.5;
      layered_t out = {0.0, 0.0, 0.0, 0.0};
      /* The blade, widening from the tip to the pivot. */
      double along = paint_clamp((py - tip_y) / (pivot_y - tip_y));
      double half = tip_half + (root_half - tip_half) * along;
      double blade = fabs(px - cx) - half;
      blade = fmax(blade, tip_y - py);
      blade = fmax(blade, py - pivot_y);
      double cover = paint_coverage(blade);
      if (cover > 0.0) {
        double ridge = 1.0 - pow(paint_clamp(fabs(px - cx) / half), 2.0);
        paint_layer(&out, paint_shade(NEEDLE, 0.7 + 0.45 * ridge), cover);
      }
      /* The counterweight below the pivot. */
      double tail = capsule(px, py, cx, pivot_y, cx, tail_y, tail_half);
      cover = paint_coverage(tail);
      if (cover > 0.0) {
        double ridge = 1.0 - pow(paint_clamp(fabs(px - cx) / tail_half), 2.0);
        paint_layer(&out, paint_shade(WEIGHT, 0.8 + 0.6 * ridge), cover);
      }
      /* The chrome hub cap over the pivot. */
      double cap = hypot(px - cx, py - pivot_y) - hub;
      cover = paint_coverage(cap);
      if (cover > 0.0) {
        double nx = (px - cx) / hub, ny = (py - pivot_y) / hub;
        double nz = sqrt(fmax(0.0, 1.0 - nx * nx - ny * ny));
        double diffuse = fmax(0.0, -0.45 * nx - 0.6 * ny + 0.66 * nz);
        double luminance = 0.3 + 0.6 * diffuse + 0.6 * pow(diffuse, 12.0);
        paint_layer(&out, paint_shade(CHROME, luminance), cover);
      }
      pixels[y * width + x] = paint_pack(&out);
    }
  }
}
