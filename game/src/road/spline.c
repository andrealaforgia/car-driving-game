#include "spline.h"

#include <math.h>

static double catmull_rom(double p0, double p1, double p2, double p3,
                          double s) {
  return 0.5 * (2.0 * p1 + (-p0 + p2) * s +
                (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * s * s +
                (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * s * s * s);
}

vec3_t closed_spline_point(const vec3_t* points, int count, double t) {
  double wrapped = fmod(t, (double)count);
  if (wrapped < 0.0) {
    wrapped += count;
  }
  int i = (int)floor(wrapped);
  double s = wrapped - i;
  vec3_t p0 = points[(i + count - 1) % count];
  vec3_t p1 = points[i % count];
  vec3_t p2 = points[(i + 1) % count];
  vec3_t p3 = points[(i + 2) % count];
  return vec3(catmull_rom(p0.x, p1.x, p2.x, p3.x, s),
              catmull_rom(p0.y, p1.y, p2.y, p3.y, s),
              catmull_rom(p0.z, p1.z, p2.z, p3.z, s));
}
