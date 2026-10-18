#include "road_view.h"

#include <math.h>
#include <stdlib.h>

#include "clip.h"
#include "road_texture.h"

#define DRAW_FAR (DRAW_DISTANCE * SEGMENT_LENGTH)
#define GRASS_EXTENT 12.0
#define RUMBLE_WIDTH (1.0 / (2.0 * LANE_COUNT))
#define LANE_MARKER_HALF_WIDTH (1.0 / (8.0 * LANE_COUNT))
#define SPLIT_IN_FOUR_WITHIN (10.0 * SEGMENT_LENGTH)
#define SPLIT_IN_TWO_WITHIN (30.0 * SEGMENT_LENGTH)
/* Rumble strips and lane markings are thinner than a pixel beyond this. */
#define DETAIL_WITHIN (120.0 * SEGMENT_LENGTH)
#define SEGMENTS_BEHIND 12
#define CAMERA_BODY_PITCH 0.02

camera_pose_t cockpit_camera_pose(const track_t* track, double position,
                                  double car_x, double car_height,
                                  double body_pitch) {
  camera_pose_t pose;
  pose.position = track_point_at(track, position, car_x);
  pose.position.y = car_height + CAMERA_HEIGHT;
  pose.yaw = track_heading_at(track, position);
  pose.pitch =
      atan(track_slope_at(track, position)) + body_pitch * CAMERA_BODY_PITCH;
  return pose;
}

void begin_road_view(road_view_t* view, const camera_t* camera,
                     const camera_pose_t* pose) {
  view->camera = *camera;
  view->pose = *pose;
  view->polygon_count = 0;
  view->sprite_count = 0;
  view->order_count = 0;
  view->horizon = horizon_y(camera, pose);
}

static bool off_screen(const view_polygon_t* polygon) {
  bool above = true, below = true, left = true, right = true;
  for (int i = 0; i < polygon->vertex_count; ++i) {
    above &= polygon->vertices[i].y < 0.0;
    below &= polygon->vertices[i].y > 1.0;
    left &= polygon->vertices[i].x < 0.0;
    right &= polygon->vertices[i].x > 1.0;
  }
  return above || below || left || right;
}

static double fog_at(double depth) {
  return fmin(1.0, fmax(0.0, depth / DRAW_FAR));
}

static void add_quad(road_view_t* view, const vec3_t corners[4],
                     const double v[2], surface_t surface, bool light) {
  if (view->polygon_count >= MAX_VIEW_POLYGONS) {
    return;
  }
  clip_vertex_t in[4];
  const double u[4] = {0.0, 1.0, 1.0, 0.0};
  const int v_index[4] = {0, 0, 1, 1};
  double farthest = -1e300;
  for (int i = 0; i < 4; ++i) {
    in[i].position = to_camera_space(&view->pose, corners[i]);
    in[i].u = u[i];
    in[i].v = v[v_index[i]];
    farthest = fmax(farthest, in[i].position.z);
  }
  if (farthest > DRAW_FAR) {
    return;
  }
  clip_vertex_t out[CLIP_MAX_VERTICES];
  int count = clip_to_near_plane(in, 4, CAMERA_NEAR_PLANE, out);
  if (count < 3 || count > POLYGON_MAX_VERTICES) {
    return;
  }
  view_polygon_t* polygon = &view->polygons[view->polygon_count];
  double depth_sum = 0.0;
  polygon->depth = 0.0;
  for (int i = 0; i < count; ++i) {
    projected_point_t p = project(&view->camera, out[i].position);
    polygon->vertices[i].x = p.x;
    polygon->vertices[i].y = p.y;
    polygon->vertices[i].u = out[i].u;
    polygon->vertices[i].v = out[i].v;
    polygon->depth = fmax(polygon->depth, out[i].position.z);
    depth_sum += out[i].position.z;
  }
  polygon->vertex_count = count;
  polygon->surface = surface;
  polygon->light = light;
  polygon->fog = fog_at(depth_sum / count);
  if (!off_screen(polygon)) {
    ++view->polygon_count;
  }
}

typedef struct {
  vec3_t start_left;
  vec3_t start_right;
  vec3_t end_left;
  vec3_t end_right;
} strip_t;

static strip_t strip_edges(const segment_t* segment, double left,
                           double right) {
  vec3_t start_side = heading_right(segment->heading);
  vec3_t end_side = heading_right(segment->end_heading);
  strip_t strip;
  strip.start_left =
      vec3_add(segment->start, vec3_scale(start_side, left * ROAD_HALF_WIDTH));
  strip.start_right =
      vec3_add(segment->start, vec3_scale(start_side, right * ROAD_HALF_WIDTH));
  strip.end_left =
      vec3_add(segment->end, vec3_scale(end_side, left * ROAD_HALF_WIDTH));
  strip.end_right =
      vec3_add(segment->end, vec3_scale(end_side, right * ROAD_HALF_WIDTH));
  return strip;
}

/* A strip of the segment between two lateral offsets (in road half widths)
 * and two fractions of its length, textured from v_near to v_far. */
static void add_strip(road_view_t* view, const segment_t* segment, double left,
                      double right, double from, double to, double v_near,
                      double v_far, surface_t surface, bool light) {
  strip_t strip = strip_edges(segment, left, right);
  vec3_t corners[4] = {vec3_lerp(strip.start_left, strip.end_left, from),
                       vec3_lerp(strip.start_right, strip.end_right, from),
                       vec3_lerp(strip.start_right, strip.end_right, to),
                       vec3_lerp(strip.start_left, strip.end_left, to)};
  double v[2] = {v_near, v_far};
  add_quad(view, corners, v, surface, light);
}

static int pieces_for(double nearest_z) {
  if (nearest_z < SPLIT_IN_FOUR_WITHIN) {
    return 4;
  }
  return nearest_z < SPLIT_IN_TWO_WITHIN ? 2 : 1;
}

static void add_segment_to_view(road_view_t* view, const segment_t* segment,
                                int index) {
  vec3_t start = to_camera_space(&view->pose, segment->start);
  vec3_t end = to_camera_space(&view->pose, segment->end);
  double reach = GRASS_EXTENT * ROAD_HALF_WIDTH;
  if (fmin(start.z, end.z) > DRAW_FAR + reach ||
      fmax(start.z, end.z) < CAMERA_NEAR_PLANE - reach) {
    return;
  }
  bool light = is_light_segment(index);
  bool detailed = fmin(start.z, end.z) < DETAIL_WITHIN;
  double span = 1.0 / ROAD_TEXTURE_SEGMENTS;
  double v0 = (index % ROAD_TEXTURE_SEGMENTS) * span;
  add_strip(view, segment, -GRASS_EXTENT, GRASS_EXTENT, 0.0, 1.0, 0.0, 1.0,
            SURFACE_GRASS, light);
  if (detailed) {
    add_strip(view, segment, -1.0 - RUMBLE_WIDTH, -1.0, 0.0, 1.0, 0.0, 1.0,
              SURFACE_RUMBLE, light);
    add_strip(view, segment, 1.0, 1.0 + RUMBLE_WIDTH, 0.0, 1.0, 0.0, 1.0,
              SURFACE_RUMBLE, light);
  }
  int pieces = pieces_for(fmin(start.z, end.z));
  for (int k = 0; k < pieces; ++k) {
    double from = (double)k / pieces;
    double to = (double)(k + 1) / pieces;
    add_strip(view, segment, -1.0, 1.0, from, to, v0 + span * from,
              v0 + span * to, SURFACE_ROAD, light);
  }
  if (!light || !detailed) {
    return;
  }
  for (int lane = 1; lane < LANE_COUNT; ++lane) {
    double centre = -1.0 + 2.0 * lane / LANE_COUNT;
    add_strip(view, segment, centre - LANE_MARKER_HALF_WIDTH,
              centre + LANE_MARKER_HALF_WIDTH, 0.0, 1.0, 0.0, 1.0, SURFACE_LANE,
              light);
  }
}

/* Segments are added from just behind the camera onwards, so when the
 * polygon budget runs out it is the far end of the lap that goes. */
void add_track_to_view(road_view_t* view, const track_t* track,
                       double position) {
  int first = segment_index_at(track, position) - SEGMENTS_BEHIND;
  for (int n = 0; n < track->count; ++n) {
    int index = ((first + n) % track->count + track->count) % track->count;
    add_segment_to_view(view, &track->segments[index], index);
  }
}

void add_billboard_to_view(road_view_t* view, const billboard_t* billboard) {
  if (view->sprite_count >= MAX_VIEW_SPRITES) {
    return;
  }
  vec3_t foot = to_camera_space(&view->pose, billboard->position);
  if (foot.z < CAMERA_NEAR_PLANE || foot.z > DRAW_FAR) {
    return;
  }
  projected_point_t p = project(&view->camera, foot);
  view_sprite_t sprite;
  sprite.x = p.x;
  sprite.y = p.y;
  sprite.width = p.scale * billboard->width * 0.5;
  sprite.height = p.scale * billboard->height * 0.5 * view->camera.aspect;
  sprite.kind = billboard->kind;
  sprite.variant = billboard->variant;
  sprite.depth = foot.z;
  sprite.fog = fog_at(foot.z);
  bool off = sprite.x + sprite.width / 2.0 < 0.0 ||
             sprite.x - sprite.width / 2.0 > 1.0 || sprite.y < 0.0 ||
             sprite.y - sprite.height > 1.0;
  if (!off) {
    view->sprites[view->sprite_count++] = sprite;
  }
}

static int farther_first(const void* a, const void* b) {
  const draw_key_t* p = a;
  const draw_key_t* q = b;
  if (p->depth != q->depth) {
    return p->depth > q->depth ? -1 : 1;
  }
  return p->id < q->id ? -1 : (p->id > q->id);
}

void finish_road_view(road_view_t* view) {
  int n = 0;
  for (int i = 0; i < view->polygon_count; ++i, ++n) {
    view->keys[n].depth = view->polygons[i].depth;
    view->keys[n].id = i;
  }
  for (int i = 0; i < view->sprite_count; ++i, ++n) {
    view->keys[n].depth = view->sprites[i].depth;
    view->keys[n].id = MAX_VIEW_POLYGONS + i;
  }
  qsort(view->keys, (size_t)n, sizeof(draw_key_t), farther_first);
  for (int i = 0; i < n; ++i) {
    view->order[i] = view->keys[i].id;
  }
  view->order_count = n;
}
