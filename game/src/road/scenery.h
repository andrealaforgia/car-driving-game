/**
 * @file scenery.h
 * @brief Trees, bushes and signs scattered beside the track from a seed,
 * and the test for driving into one of them.
 */

#ifndef GAME_SRC_ROAD_SCENERY_H_
#define GAME_SRC_ROAD_SCENERY_H_

#include <stdbool.h>

#include "billboard.h"
#include "car.h"
#include "track.h"

#define MAX_SCENERY 2400

typedef struct {
  billboard_t billboard;
  int segment;
  double lateral; /* road half widths from the centre, right is positive */
} scenery_item_t;

typedef struct {
  scenery_item_t* items; /* in segment order */
  int count;
} scenery_t;

scenery_t place_scenery(const track_t* track, unsigned seed);
void destroy_scenery(scenery_t* scenery);

bool scenery_blocks(const scenery_t* scenery, const car_t* car);

#endif  // GAME_SRC_ROAD_SCENERY_H_
