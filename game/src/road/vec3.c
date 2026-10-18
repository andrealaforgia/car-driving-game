#include "vec3.h"

#include <math.h>

vec3_t vec3(double x, double y, double z) {
  vec3_t v = {x, y, z};
  return v;
}

vec3_t vec3_add(vec3_t a, vec3_t b) {
  return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

vec3_t vec3_sub(vec3_t a, vec3_t b) {
  return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

vec3_t vec3_scale(vec3_t a, double factor) {
  return vec3(a.x * factor, a.y * factor, a.z * factor);
}

vec3_t vec3_lerp(vec3_t a, vec3_t b, double t) {
  return vec3_add(a, vec3_scale(vec3_sub(b, a), t));
}

double vec3_length(vec3_t a) { return sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

double vec3_distance(vec3_t a, vec3_t b) { return vec3_length(vec3_sub(a, b)); }
