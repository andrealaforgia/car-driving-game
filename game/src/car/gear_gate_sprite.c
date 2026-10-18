#include "gear_gate_sprite.h"

#include <math.h>

#include "dashboard.h"
#include "sprite_paint.h"

/* Plate layout, as fractions of its size. */
#define SLOT_MARGIN_X 0.13
#define SLOT_TOP 0.20
#define SLOT_BOTTOM 0.80
#define SLOT_HALF_WIDTH 0.038 /* of height */
#define CORNER_RADIUS 0.07    /* of height */
#define BEVEL 0.035           /* of height */

/* Lever layout, as fractions of its height. */
#define KNOB_RADIUS 0.15
#define SHAFT_HALF_WIDTH 0.032
#define BOOT_RX 0.19
#define BOOT_RY 0.05
#define SHADOW_RX 0.26
#define SHADOW_RY 0.06

static const rgb_t ALUMINIUM = {0.66, 0.67, 0.70};
static const rgb_t GROOVE = {0.10, 0.10, 0.11};
static const rgb_t CHROME = {0.80, 0.82, 0.88};
static const rgb_t KNOB = {0.11, 0.10, 0.10};
static const rgb_t BOOT = {0.16, 0.14, 0.13};
static const rgb_t SHADOW = {0.0, 0.0, 0.0};

double gate_slot_x(double column) {
  return SLOT_MARGIN_X +
         (1.0 - 2.0 * SLOT_MARGIN_X) * column / (GEAR_GATE_COLUMNS - 1);
}

double gate_slot_y(double row) { return 0.5 + row * (0.5 - SLOT_TOP); }

/* Signed distance to a rounded rectangle centred on the origin. */
static double rounded_box(double px, double py, double half_w, double half_h,
                          double radius) {
  double qx = fabs(px) - half_w + radius;
  double qy = fabs(py) - half_h + radius;
  double outside = hypot(fmax(qx, 0.0), fmax(qy, 0.0));
  return outside + fmin(fmax(qx, qy), 0.0) - radius;
}

/* Signed distance to a line segment thickened into a capsule. */
static double capsule(double px, double py, double ax, double ay, double bx,
                      double by, double radius) {
  double abx = bx - ax, aby = by - ay;
  double t = ((px - ax) * abx + (py - ay) * aby) / (abx * abx + aby * aby);
  t = paint_clamp(t);
  return hypot(px - (ax + abx * t), py - (ay + aby * t)) - radius;
}

/* Distance to the nearest groove of the H, and the direction out of it. */
static double groove_distance(double px, double py, int width, int height) {
  double half = SLOT_HALF_WIDTH * height;
  double top = SLOT_TOP * height, bottom = SLOT_BOTTOM * height;
  double d =
      capsule(px, py, gate_slot_x(0) * width, height / 2.0,
              gate_slot_x(GEAR_GATE_COLUMNS - 1) * width, height / 2.0, half);
  for (int column = 0; column < GEAR_GATE_COLUMNS; ++column) {
    double x = gate_slot_x(column) * width;
    d = fmin(d, capsule(px, py, x, top, x, bottom, half));
  }
  return d;
}

void render_gear_gate_pixels(uint32_t* pixels, int width, int height) {
  double corner = CORNER_RADIUS * height;
  double bevel = BEVEL * height;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double px = x + 0.5, py = y + 0.5;
      layered_t out = {0.0, 0.0, 0.0, 0.0};
      double edge = rounded_box(px - width / 2.0, py - height / 2.0,
                                width / 2.0, height / 2.0, corner);
      double cover = paint_coverage(edge);
      if (cover > 0.0) {
        /* Brushed: grain runs along the rows; lit from the top left. */
        double light = 1.0 - 0.18 * ((px / width) + (py / height) - 1.0);
        double brush =
            1.0 + 0.07 * paint_grain(y, x / 7) + 0.03 * paint_grain(x, y);
        double luminance = light * brush;
        double depth = -edge; /* pixels in from the edge */
        if (depth < bevel) {
          double inwards = 1.0 - depth / bevel;
          bool lit_side = (px / width + py / height) < 1.0;
          luminance *= lit_side ? 1.0 + 0.35 * inwards : 1.0 - 0.35 * inwards;
        }
        rgb_t colour = paint_shade(ALUMINIUM, luminance);
        double groove = groove_distance(px, py, width, height);
        double inside = paint_coverage(groove);
        if (inside > 0.0) {
          /* Dark floor, darker still against the top-left wall. */
          double wall = paint_clamp(-groove / (SLOT_HALF_WIDTH * height));
          double dx = groove_distance(px + 1.0, py, width, height) - groove;
          double dy = groove_distance(px, py + 1.0, width, height) - groove;
          double shade = 0.7 + 0.5 * wall - 0.35 * paint_clamp(-(dx + dy));
          colour = paint_mix(colour, paint_shade(GROOVE, shade), inside);
        } else if (groove < 1.5) {
          /* A bright lip on the lower-right edge of each slot. */
          double dx = groove_distance(px + 1.0, py, width, height) - groove;
          double dy = groove_distance(px, py + 1.0, width, height) - groove;
          double lip = paint_clamp(-(dx + dy) * 0.8);
          colour = paint_shade(colour, 1.0 + 0.5 * lip);
        }
        paint_layer(&out, colour, cover);
      }
      pixels[y * width + x] = paint_pack(&out);
    }
  }
}

void render_gear_lever_pixels(uint32_t* pixels, int width, int height) {
  double cx = width / 2.0;
  double knob_y = LEVER_KNOB_Y * height;
  double knob_r = KNOB_RADIUS * height;
  double base_y = LEVER_BASE_Y * height;
  double shaft_w = SHAFT_HALF_WIDTH * height;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      double px = x + 0.5, py = y + 0.5;
      layered_t out = {0.0, 0.0, 0.0, 0.0};
      /* Soft shadow on the plate around the boot. */
      double shadow = hypot((px - cx) / (SHADOW_RX * height),
                            (py - base_y) / (SHADOW_RY * height));
      if (shadow < 1.0) {
        paint_layer(&out, SHADOW, 0.45 * (1.0 - shadow * shadow));
      }
      /* Chromed shaft, tapering a little towards the knob. */
      double taper = 0.85 + 0.15 * (py - knob_y) / (base_y - knob_y);
      double shaft = capsule(px, py, cx, knob_y, cx, base_y, shaft_w * taper);
      double cover = paint_coverage(shaft);
      if (cover > 0.0) {
        double n = paint_clamp(fabs(px - cx) / (shaft_w * taper));
        double round = sqrt(1.0 - n * n);
        double stripe =
            exp(-pow((px - cx + 0.45 * shaft_w) / (0.25 * shaft_w), 2.0));
        double luminance = 0.35 + 0.45 * round + 0.6 * stripe;
        paint_layer(&out, paint_shade(CHROME, luminance), cover);
      }
      /* Leather boot gathered at the base. */
      double boot =
          hypot((px - cx) / (BOOT_RX * height),
                (py - base_y + BOOT_RY * height * 0.6) / (BOOT_RY * height));
      if (boot < 1.0) {
        double fold = 1.0 + 0.25 * sin((px - cx) / (BOOT_RX * height) * 9.0);
        double luminance = (0.7 + 0.6 * (1.0 - boot)) * fold;
        paint_layer(&out, paint_shade(BOOT, luminance),
                    paint_coverage((boot - 1.0) * BOOT_RY * height));
      }
      /* Glossy black knob, a sphere lit from the top left. */
      double knob = hypot(px - cx, py - knob_y) - knob_r;
      cover = paint_coverage(knob);
      if (cover > 0.0) {
        double nx = (px - cx) / knob_r, ny = (py - knob_y) / knob_r;
        double nz = sqrt(fmax(0.0, 1.0 - nx * nx - ny * ny));
        double diffuse = fmax(0.0, -0.45 * nx - 0.6 * ny + 0.66 * nz);
        double specular = pow(diffuse, 24.0);
        double rim = pow(1.0 - nz, 3.0);
        double luminance = 0.5 + 1.2 * diffuse + 0.3 * rim;
        rgb_t colour = paint_shade(KNOB, luminance);
        colour = paint_mix(colour, CHROME, 0.9 * specular);
        paint_layer(&out, colour, cover);
      }
      pixels[y * width + x] = paint_pack(&out);
    }
  }
}
