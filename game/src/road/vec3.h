/**
 * @file vec3.h
 * @brief Three-component vectors for world and camera space. x points
 * right, y up and z forward.
 */

#ifndef GAME_SRC_ROAD_VEC3_H_
#define GAME_SRC_ROAD_VEC3_H_

typedef struct {
  double x;
  double y;
  double z;
} vec3_t;

vec3_t vec3(double x, double y, double z);
vec3_t vec3_add(vec3_t a, vec3_t b);
vec3_t vec3_sub(vec3_t a, vec3_t b);
vec3_t vec3_scale(vec3_t a, double factor);
vec3_t vec3_lerp(vec3_t a, vec3_t b, double t);
double vec3_length(vec3_t a);
double vec3_distance(vec3_t a, vec3_t b);

#endif  // GAME_SRC_ROAD_VEC3_H_
