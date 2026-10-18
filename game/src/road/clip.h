/**
 * @file clip.h
 * @brief Clips a convex polygon in camera space against the near plane,
 * so nothing behind the camera is ever projected.
 */

#ifndef GAME_SRC_ROAD_CLIP_H_
#define GAME_SRC_ROAD_CLIP_H_

#include "vec3.h"

#define CLIP_MAX_VERTICES 8

typedef struct {
  vec3_t position;
  double u;
  double v;
} clip_vertex_t;

/* Writes the clipped polygon to out (room for count + 1 vertices) and
 * returns its vertex count, which is 0 when nothing is in front. */
int clip_to_near_plane(const clip_vertex_t* in, int count, double near_z,
                       clip_vertex_t* out);

#endif  // GAME_SRC_ROAD_CLIP_H_
