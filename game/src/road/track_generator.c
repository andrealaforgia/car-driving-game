#include "track_generator.h"

#include <math.h>

#include "game_constants.h"
#include "geometry.h"
#include "spline.h"

#define RING_RADIUS 34000.0
#define CONTROL_POINTS 24
#define HARMONICS 3
#define MAX_HEIGHT_STEP 2200.0
#define MAX_CONTROL_HEIGHT 9000.0
#define SAMPLES_PER_SPAN 32
#define MAX_SAMPLES (CONTROL_POINTS * SAMPLES_PER_SPAN + 1)
#define MAX_ATTEMPTS 200

typedef struct {
  unsigned state;
} dice_t;

static unsigned roll(dice_t* dice, unsigned sides) {
  dice->state = dice->state * 1664525u + 1013904223u;
  return (dice->state >> 8) % sides;
}

static double roll_fraction(dice_t* dice) { return roll(dice, 10000) / 9999.0; }

static double roll_signed(dice_t* dice) {
  return roll_fraction(dice) * 2.0 - 1.0;
}

static double clamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

/* Amplitude ranges for the second, third and fourth harmonics of the
 * ring's radius: the third makes dents that bend the other way. */
static const double SMALLEST_AMPLITUDE[HARMONICS] = {0.0, 0.14, 0.0};
static const double LARGEST_AMPLITUDE[HARMONICS] = {0.12, 0.24, 0.07};

static void shape_the_ring(dice_t* dice, vec3_t* points) {
  double amplitude[HARMONICS];
  double phase[HARMONICS];
  for (int h = 0; h < HARMONICS; ++h) {
    amplitude[h] =
        SMALLEST_AMPLITUDE[h] +
        (LARGEST_AMPLITUDE[h] - SMALLEST_AMPLITUDE[h]) * roll_fraction(dice);
    phase[h] = roll_fraction(dice) * 2.0 * M_PI;
  }
  for (int i = 0; i < CONTROL_POINTS; ++i) {
    double angle = 2.0 * M_PI * i / CONTROL_POINTS;
    double radius = 1.0;
    for (int h = 0; h < HARMONICS; ++h) {
      radius += amplitude[h] * sin((h + 2) * angle + phase[h]);
    }
    radius *= RING_RADIUS;
    points[i] = vec3(radius * sin(angle), 0.0, radius * cos(angle));
  }
}

static void wander_heights(dice_t* dice, vec3_t* points) {
  double height[CONTROL_POINTS];
  height[0] = 0.0;
  for (int i = 1; i < CONTROL_POINTS; ++i) {
    height[i] = clamp(height[i - 1] + roll_signed(dice) * MAX_HEIGHT_STEP,
                      -MAX_CONTROL_HEIGHT, MAX_CONTROL_HEIGHT);
  }
  /* Tilt the whole profile so the last point comes back close to the
   * first and the lap closes with a gentle slope. */
  double mismatch = height[CONTROL_POINTS - 1];
  for (int i = 0; i < CONTROL_POINTS; ++i) {
    points[i].y = height[i] - mismatch * i / CONTROL_POINTS;
  }
}

typedef struct {
  vec3_t position[MAX_SAMPLES];
  double distance[MAX_SAMPLES];
  int count;
} samples_t;

static void sample_spline(const vec3_t* control, samples_t* samples) {
  samples->count = CONTROL_POINTS * SAMPLES_PER_SPAN + 1;
  for (int i = 0; i < samples->count; ++i) {
    double t = (double)i / SAMPLES_PER_SPAN;
    samples->position[i] = closed_spline_point(control, CONTROL_POINTS, t);
    samples->distance[i] = i == 0 ? 0.0
                                  : samples->distance[i - 1] +
                                        vec3_distance(samples->position[i - 1],
                                                      samples->position[i]);
  }
}

static int segment_count_for(double length) {
  int count = (int)lround(length / SEGMENT_LENGTH);
  count = (count + TRACK_SEGMENT_MULTIPLE / 2) / TRACK_SEGMENT_MULTIPLE *
          TRACK_SEGMENT_MULTIPLE;
  return count < TRACK_SEGMENT_MULTIPLE ? TRACK_SEGMENT_MULTIPLE : count;
}

static vec3_t point_at_distance(const samples_t* samples, double distance,
                                int* cursor) {
  while (*cursor < samples->count - 2 &&
         samples->distance[*cursor + 1] < distance) {
    ++*cursor;
  }
  double span = samples->distance[*cursor + 1] - samples->distance[*cursor];
  double t = span > 0.0 ? (distance - samples->distance[*cursor]) / span : 0.0;
  return vec3_lerp(samples->position[*cursor], samples->position[*cursor + 1],
                   t);
}

static bool within_limits(const track_t* track) {
  bool bends_left = false, bends_right = false, has_hills = false;
  for (int i = 0; i < track->count; ++i) {
    const segment_t* segment = &track->segments[i];
    if (fabs(segment->curve) > TRACK_MAX_CURVE ||
        fabs(segment_slope(segment)) > TRACK_MAX_SLOPE ||
        fabs(segment->end.y) > TRACK_MAX_HEIGHT) {
      return false;
    }
    bends_left |= segment->curve < -TRACK_MIN_BEND;
    bends_right |= segment->curve > TRACK_MIN_BEND;
    has_hills |= fabs(segment->end.y) > TRACK_MIN_HILL;
  }
  return bends_left && bends_right && has_hills;
}

static void build_candidate(dice_t* dice, track_t* track) {
  vec3_t control[CONTROL_POINTS];
  shape_the_ring(dice, control);
  wander_heights(dice, control);
  samples_t samples;
  sample_spline(control, &samples);
  double length = samples.distance[samples.count - 1];
  int count = segment_count_for(length);
  if (count > track->capacity) {
    track->count = 0;
    return;
  }
  vec3_t points[MAX_TRACK_SEGMENTS];
  int cursor = 0;
  for (int i = 0; i < count; ++i) {
    vec3_t point = point_at_distance(&samples, length * i / count, &cursor);
    points[i] = vec3_sub(point, control[0]);
  }
  build_closed_track(track, points, count);
}

track_t generate_track(unsigned seed) {
  dice_t dice = {seed};
  track_t track = create_track(MAX_TRACK_SEGMENTS);
  if (track.capacity == 0) {
    return track;
  }
  for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
    build_candidate(&dice, &track);
    if (track.count > 0 && within_limits(&track)) {
      return track;
    }
  }
  track.count = 0;
  track.length = 0.0;
  return track;
}
