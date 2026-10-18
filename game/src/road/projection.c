#include "projection.h"

#include <math.h>

#include "game_constants.h"
#include "geometry.h"

camera_t create_camera(double field_of_view_degrees, double aspect) {
  camera_t camera;
  camera.depth = 1.0 / tan((field_of_view_degrees / 2.0) * M_PI / 180.0);
  camera.aspect = aspect;
  return camera;
}

vec3_t to_camera_space(const camera_pose_t* pose, vec3_t world) {
  vec3_t d = vec3_sub(world, pose->position);
  double cos_yaw = cos(pose->yaw);
  double sin_yaw = sin(pose->yaw);
  double x = d.x * cos_yaw - d.z * sin_yaw;
  double z = d.x * sin_yaw + d.z * cos_yaw;
  double cos_pitch = cos(pose->pitch);
  double sin_pitch = sin(pose->pitch);
  double y = d.y * cos_pitch - z * sin_pitch;
  z = d.y * sin_pitch + z * cos_pitch;
  return vec3(x, y, z);
}

bool is_in_front_of_camera(vec3_t camera_space) {
  return camera_space.z >= CAMERA_NEAR_PLANE;
}

projected_point_t project(const camera_t* camera, vec3_t p) {
  double scale = camera->depth / p.z;
  projected_point_t projected;
  projected.x = 0.5 + scale * p.x * 0.5;
  projected.y = 0.5 - scale * p.y * 0.5 * camera->aspect;
  projected.scale = scale;
  return projected;
}

double horizon_y(const camera_t* camera, const camera_pose_t* pose) {
  return 0.5 + camera->depth * tan(pose->pitch) * 0.5 * camera->aspect;
}
