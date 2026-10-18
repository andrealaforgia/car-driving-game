/**
 * @file track_generator.h
 * @brief Builds a random closed circuit from a seed.
 *
 * The lap is a smooth loop around a ring whose radius wobbles with a few
 * random harmonics, so it bulges and dents and bends both ways. Control
 * points along it get heights that wander up and down within limits, and a
 * closed spline through them is sampled into segments of equal length,
 * rounded to a multiple of the rumble and texture periods so the lap wraps
 * seamlessly. Candidates that break the bend, slope or height limits are
 * rejected and rolled again, so the same seed always gives the same
 * circuit and every circuit is within the limits below.
 */

#ifndef GAME_SRC_ROAD_TRACK_GENERATOR_H_
#define GAME_SRC_ROAD_TRACK_GENERATOR_H_

#include "track.h"

#define TRACK_SEGMENT_MULTIPLE 12
/* Radians of turn per segment: the tightest bend has a radius of about
 * 3300 units, the gentlest one that still counts as a bend about 16000. */
#define TRACK_MAX_CURVE 0.06
#define TRACK_MIN_BEND 0.012
#define TRACK_MAX_SLOPE 0.45
#define TRACK_MAX_HEIGHT 14000.0
#define TRACK_MIN_HILL 2000.0

track_t generate_track(unsigned seed);

#endif  // GAME_SRC_ROAD_TRACK_GENERATOR_H_
