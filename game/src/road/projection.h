/**
 * @file projection.h
 * @brief A perspective camera with a position, a yaw and a pitch.
 *
 * World points are moved into camera space (x right, y up, z forward, all
 * relative to the camera) and then projected onto a normalised screen: x
 * and y run from 0 to 1 with the origin at the top left, and scale is the
 * factor that turns a world length at that depth into a fraction of the
 * screen width. The renderer multiplies by the real resolution.
 */

#ifndef GAME_SRC_ROAD_PROJECTION_H_
#define GAME_SRC_ROAD_PROJECTION_H_

#include <stdbool.h>

#include "vec3.h"

typedef struct {
  double depth;  /* distance of the screen plane for a unit half width */
  double aspect; /* screen width over height */
} camera_t;

typedef struct {
  vec3_t position;
  double yaw;   /* radians from +z, turning right is positive */
  double pitch; /* radians, looking up is positive */
} camera_pose_t;

typedef struct {
  double x;
  double y;
  double scale;
} projected_point_t;

camera_t create_camera(double field_of_view_degrees, double aspect);

vec3_t to_camera_space(const camera_pose_t* pose, vec3_t world);

bool is_in_front_of_camera(vec3_t camera_space);

projected_point_t project(const camera_t* camera, vec3_t camera_space);

/* Screen y of the line where level ground meets the sky. */
double horizon_y(const camera_t* camera, const camera_pose_t* pose);

#endif  // GAME_SRC_ROAD_PROJECTION_H_
