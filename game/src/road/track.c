#include "track.h"

#include <math.h>
#include <stdlib.h>

#include "game_constants.h"
#include "geometry.h"

track_t create_track(int capacity) {
  track_t track;
  track.segments = calloc((size_t)capacity, sizeof(segment_t));
  track.capacity = track.segments ? capacity : 0;
  track.count = 0;
  track.length = 0.0;
  return track;
}

void destroy_track(track_t* track) {
  free(track->segments);
  track->segments = NULL;
  track->count = 0;
  track->capacity = 0;
  track->length = 0.0;
}

vec3_t heading_forward(double heading) {
  return vec3(sin(heading), 0.0, cos(heading));
}

vec3_t heading_right(double heading) {
  return vec3(cos(heading), 0.0, -sin(heading));
}

double wrap_angle(double angle) {
  while (angle > M_PI) {
    angle -= 2.0 * M_PI;
  }
  while (angle < -M_PI) {
    angle += 2.0 * M_PI;
  }
  return angle;
}

static const segment_t* last_segment(const track_t* track) {
  return track->count == 0 ? NULL : &track->segments[track->count - 1];
}

static void add_segment(track_t* track, double curve, double y_end) {
  if (track->count >= track->capacity) {
    return;
  }
  const segment_t* previous = last_segment(track);
  segment_t* segment = &track->segments[track->count];
  segment->z = track->count * SEGMENT_LENGTH;
  segment->start = previous ? previous->end : vec3(0.0, 0.0, 0.0);
  segment->heading = previous ? previous->end_heading : 0.0;
  segment->curve = curve;
  segment->end_heading = segment->heading + curve;
  vec3_t advance = vec3_scale(heading_forward(segment->heading + curve / 2.0),
                              SEGMENT_LENGTH);
  segment->end = vec3_add(segment->start, advance);
  segment->end.y = y_end;
  ++track->count;
  track->length = track->count * SEGMENT_LENGTH;
}

static double ease_in(double from, double to, double progress) {
  return from + (to - from) * progress * progress;
}

static double ease_in_out(double from, double to, double progress) {
  return from + (to - from) * (0.5 - cos(progress * M_PI) / 2.0);
}

static double last_height(const track_t* track) {
  const segment_t* previous = last_segment(track);
  return previous ? previous->end.y : 0.0;
}

void add_road(track_t* track, int enter, int hold, int leave, double curve,
              double height_change) {
  double start_height = last_height(track);
  double end_height = start_height + height_change;
  int total = enter + hold + leave;
  int n = 0;
  for (int i = 0; i < enter; ++i, ++n) {
    add_segment(track, ease_in(0.0, curve, (double)i / enter),
                ease_in_out(start_height, end_height, (double)(n + 1) / total));
  }
  for (int i = 0; i < hold; ++i, ++n) {
    add_segment(track, curve,
                ease_in_out(start_height, end_height, (double)(n + 1) / total));
  }
  for (int i = 0; i < leave; ++i, ++n) {
    add_segment(track, ease_in_out(curve, 0.0, (double)i / leave),
                ease_in_out(start_height, end_height, (double)(n + 1) / total));
  }
}

void add_straight(track_t* track, int segment_count) {
  add_road(track, 0, segment_count, 0, 0.0, 0.0);
}

void add_curve(track_t* track, int enter, int hold, int leave, double curve) {
  add_road(track, enter, hold, leave, curve, 0.0);
}

static double direction_between(vec3_t from, vec3_t to) {
  return atan2(to.x - from.x, to.z - from.z);
}

void build_closed_track(track_t* track, const vec3_t* points, int count) {
  if (count > track->capacity || count < 3) {
    track->count = 0;
    track->length = 0.0;
    return;
  }
  track->count = count;
  track->length = count * SEGMENT_LENGTH;
  for (int i = 0; i < count; ++i) {
    segment_t* segment = &track->segments[i];
    vec3_t before = points[(i + count - 1) % count];
    vec3_t after = points[(i + 1) % count];
    double incoming = direction_between(before, points[i]);
    double outgoing = direction_between(points[i], after);
    segment->z = i * SEGMENT_LENGTH;
    segment->start = points[i];
    segment->end = after;
    segment->heading = incoming + wrap_angle(outgoing - incoming) / 2.0;
  }
  for (int i = 0; i < count; ++i) {
    segment_t* segment = &track->segments[i];
    double next_heading = track->segments[(i + 1) % count].heading;
    segment->end_heading =
        segment->heading + wrap_angle(next_heading - segment->heading);
    segment->curve = segment->end_heading - segment->heading;
  }
}

static double wrap(double position, double length) {
  double wrapped = fmod(position, length);
  return wrapped < 0.0 ? wrapped + length : wrapped;
}

int segment_index_at(const track_t* track, double position) {
  double wrapped = wrap(position, track->length);
  return (int)floor(wrapped / SEGMENT_LENGTH) % track->count;
}

static double fraction_at(const track_t* track, const segment_t* segment,
                          double position) {
  double wrapped = wrap(position, track->length);
  return (wrapped - segment->z) / SEGMENT_LENGTH;
}

double track_height_at(const track_t* track, double position) {
  const segment_t* segment =
      &track->segments[segment_index_at(track, position)];
  double fraction = fraction_at(track, segment, position);
  return segment->start.y + (segment->end.y - segment->start.y) * fraction;
}

double track_heading_at(const track_t* track, double position) {
  const segment_t* segment =
      &track->segments[segment_index_at(track, position)];
  double fraction = fraction_at(track, segment, position);
  return segment->heading + segment->curve * fraction;
}

vec3_t track_point_at(const track_t* track, double position, double lateral) {
  const segment_t* segment =
      &track->segments[segment_index_at(track, position)];
  double fraction = fraction_at(track, segment, position);
  vec3_t centre = vec3_lerp(segment->start, segment->end, fraction);
  double heading = segment->heading + segment->curve * fraction;
  return vec3_add(
      centre, vec3_scale(heading_right(heading), lateral * ROAD_HALF_WIDTH));
}

double track_slope_at(const track_t* track, double position) {
  int index = segment_index_at(track, position);
  const segment_t* segment = &track->segments[index];
  const segment_t* before =
      &track->segments[(index + track->count - 1) % track->count];
  const segment_t* after = &track->segments[(index + 1) % track->count];
  double at_start = (segment_slope(before) + segment_slope(segment)) / 2.0;
  double at_end = (segment_slope(segment) + segment_slope(after)) / 2.0;
  double fraction = fraction_at(track, segment, position);
  return at_start + (at_end - at_start) * fraction;
}

double segment_slope(const segment_t* segment) {
  return (segment->end.y - segment->start.y) / SEGMENT_LENGTH;
}

bool is_light_segment(int index) { return (index / RUMBLE_LENGTH) % 2 == 0; }
