/**
 * @file track.h
 * @brief A track is a loop of segments with real positions in the world.
 *
 * Each segment runs from its start point to its end point along the road's
 * centreline. Its heading is the direction the road faces at its start
 * (radians from +z, turning right is positive); at a join the heading
 * bisects the segments either side so the road edges meet cleanly. The
 * curve is how far the heading turns over the segment and feeds the car's
 * physics. The distance z along the track is nominal: every segment counts
 * as SEGMENT_LENGTH for the car's position whatever its real length.
 */

#ifndef GAME_SRC_ROAD_TRACK_H_
#define GAME_SRC_ROAD_TRACK_H_

#include <stdbool.h>

#include "vec3.h"

typedef struct {
  double z;
  vec3_t start;
  vec3_t end;
  double heading;
  double end_heading;
  double curve;
} segment_t;

typedef struct {
  segment_t* segments;
  int count;
  int capacity;
  double length;
} track_t;

track_t create_track(int capacity);
void destroy_track(track_t* track);

/* Builders for hand-made tracks: curve is radians per segment. */
void add_road(track_t* track, int enter, int hold, int leave, double curve,
              double height_change);
void add_straight(track_t* track, int segment_count);
void add_curve(track_t* track, int enter, int hold, int leave, double curve);

/* Replaces the track with a closed loop through the given points. */
void build_closed_track(track_t* track, const vec3_t* points, int count);

int segment_index_at(const track_t* track, double position);
double track_height_at(const track_t* track, double position);
double track_heading_at(const track_t* track, double position);
/* Slope blended across segment joins so the camera does not jolt. */
double track_slope_at(const track_t* track, double position);
/* lateral is in road half widths: 1 is the right-hand edge. */
vec3_t track_point_at(const track_t* track, double position, double lateral);
double segment_slope(const segment_t* segment);
bool is_light_segment(int index);

vec3_t heading_forward(double heading);
vec3_t heading_right(double heading);
double wrap_angle(double angle);

#endif  // GAME_SRC_ROAD_TRACK_H_
