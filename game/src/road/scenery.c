#include "scenery.h"

#include <math.h>
#include <stdlib.h>

#include "game_constants.h"

#define SIDE_CHANCE_PERCENT 32
#define SIGN_EVERY_SEGMENTS 45

typedef struct {
  unsigned state;
} dice_t;

static unsigned roll(dice_t* dice, unsigned sides) {
  dice->state = dice->state * 1664525u + 1013904223u;
  return (dice->state >> 8) % sides;
}

static double roll_fraction(dice_t* dice) { return roll(dice, 10000) / 9999.0; }

static billboard_t pick_billboard(dice_t* dice, int segment) {
  billboard_t billboard;
  billboard.position = vec3(0.0, 0.0, 0.0);
  unsigned choice = roll(dice, 100);
  if (segment % SIGN_EVERY_SEGMENTS == 0) {
    billboard.kind = BILLBOARD_SIGN;
    billboard.variant = (int)roll(dice, 2);
    billboard.width = 900.0;
    billboard.height = 675.0;
  } else if (choice < 70) {
    double size = 0.8 + 0.4 * roll_fraction(dice);
    billboard.kind = BILLBOARD_TREE;
    billboard.variant = (int)roll(dice, 2);
    billboard.width = 700.0 * size;
    billboard.height = 1120.0 * size;
  } else {
    billboard.kind = BILLBOARD_BUSH;
    billboard.variant = 0;
    billboard.width = 480.0;
    billboard.height = 300.0;
  }
  return billboard;
}

static void place_item(scenery_t* scenery, const track_t* track, dice_t* dice,
                       int segment, double side) {
  if (scenery->count >= MAX_SCENERY) {
    return;
  }
  scenery_item_t* item = &scenery->items[scenery->count++];
  item->segment = segment;
  item->lateral = side * (SCENERY_NEAREST_LATERAL +
                          (SCENERY_FARTHEST_LATERAL - SCENERY_NEAREST_LATERAL) *
                              roll_fraction(dice));
  item->billboard = pick_billboard(dice, segment);
  item->billboard.position =
      track_point_at(track, segment * SEGMENT_LENGTH, item->lateral);
}

scenery_t place_scenery(const track_t* track, unsigned seed) {
  scenery_t scenery;
  scenery.items = calloc(MAX_SCENERY, sizeof(scenery_item_t));
  scenery.count = 0;
  if (!scenery.items) {
    return scenery;
  }
  dice_t dice = {seed};
  for (int segment = 0; segment < track->count; ++segment) {
    if (roll(&dice, 100) < SIDE_CHANCE_PERCENT) {
      place_item(&scenery, track, &dice, segment, -1.0);
    }
    if (roll(&dice, 100) < SIDE_CHANCE_PERCENT) {
      place_item(&scenery, track, &dice, segment, 1.0);
    }
  }
  return scenery;
}

void destroy_scenery(scenery_t* scenery) {
  free(scenery->items);
  scenery->items = NULL;
  scenery->count = 0;
}

bool scenery_blocks(const scenery_t* scenery, const car_t* car) {
  for (int i = 0; i < scenery->count; ++i) {
    const scenery_item_t* item = &scenery->items[i];
    double along = car->position - item->segment * SEGMENT_LENGTH;
    if (fabs(along) > SCENERY_HIT_RANGE) {
      continue;
    }
    double reach =
        CAR_HALF_WIDTH + item->billboard.width / (2.0 * ROAD_HALF_WIDTH);
    if (fabs(car->x - item->lateral) < reach) {
      return true;
    }
  }
  return false;
}
