/**
 * @file road_view.h
 * @brief Turns the track and the things standing beside it into screen
 * polygons and sprites, sorted far to near for a painter's draw.
 *
 * Pure: no SDL. Every strip of road, verge, rumble and lane marking is a
 * quad in the world, moved into camera space, clipped against the near
 * plane and projected. The road nearest the camera is split lengthwise so
 * the affine texturing does not warp.
 */

#ifndef GAME_SRC_ROAD_ROAD_VIEW_H_
#define GAME_SRC_ROAD_ROAD_VIEW_H_

#include <stdbool.h>

#include "billboard.h"
#include "game_constants.h"
#include "projection.h"
#include "track.h"

#define MAX_VIEW_POLYGONS 3000
#define MAX_VIEW_SPRITES 600
#define POLYGON_MAX_VERTICES 5

typedef enum {
  SURFACE_GRASS,
  SURFACE_RUMBLE,
  SURFACE_ROAD,
  SURFACE_LANE
} surface_t;

typedef struct {
  double x;
  double y;
  double u;
  double v;
} screen_vertex_t;

typedef struct {
  screen_vertex_t vertices[POLYGON_MAX_VERTICES];
  int vertex_count;
  surface_t surface;
  bool light;
  double depth; /* camera-space z of the farthest vertex */
  double fog;   /* 0 close by, 1 at the draw distance */
  int segment;  /* index of the track segment it belongs to */
} view_polygon_t;

typedef struct {
  double x; /* bottom centre, fractions of the screen */
  double y;
  double width;
  double height;
  billboard_kind_t kind;
  int variant;
  double depth;
  double fog;
} view_sprite_t;

typedef struct {
  double depth;
  int id;
} draw_key_t;

typedef struct {
  camera_t camera;
  camera_pose_t pose;
  view_polygon_t polygons[MAX_VIEW_POLYGONS];
  int polygon_count;
  view_sprite_t sprites[MAX_VIEW_SPRITES];
  int sprite_count;
  draw_key_t keys[MAX_VIEW_POLYGONS + MAX_VIEW_SPRITES];
  /* Draw order, far to near. Ids below MAX_VIEW_POLYGONS index polygons;
   * the rest index sprites, offset by MAX_VIEW_POLYGONS. */
  int order[MAX_VIEW_POLYGONS + MAX_VIEW_SPRITES];
  int order_count;
  double horizon;
  int current_segment; /* tag for polygons being added */
} road_view_t;

camera_pose_t cockpit_camera_pose(const track_t* track, double position,
                                  double car_x, double car_height,
                                  double body_pitch);

void begin_road_view(road_view_t* view, const camera_t* camera,
                     const camera_pose_t* pose);
void add_track_to_view(road_view_t* view, const track_t* track,
                       double position);

/* The rear-view mirror: a pose at the car looking back down the road,
 * the track added from just ahead of the car backwards, and a flip of
 * the finished view so what is on the car's right stays on the right. */
camera_pose_t mirror_camera_pose(const track_t* track, double position,
                                 double car_x, double car_height);
void add_track_behind_to_view(road_view_t* view, const track_t* track,
                              double position);
void mirror_road_view(road_view_t* view);
void add_billboard_to_view(road_view_t* view, const billboard_t* billboard);
void finish_road_view(road_view_t* view);

#endif  // GAME_SRC_ROAD_ROAD_VIEW_H_
