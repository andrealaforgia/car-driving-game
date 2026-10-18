/**
 * @file spline.h
 * @brief A closed Catmull-Rom spline through a loop of control points.
 *
 * The parameter t runs from 0 at the first point to count at the first
 * point again; integer values land on the control points.
 */

#ifndef GAME_SRC_ROAD_SPLINE_H_
#define GAME_SRC_ROAD_SPLINE_H_

#include "vec3.h"

vec3_t closed_spline_point(const vec3_t* points, int count, double t);

#endif  // GAME_SRC_ROAD_SPLINE_H_
