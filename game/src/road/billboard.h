/**
 * @file billboard.h
 * @brief A flat sprite standing in the world: trees, signs and cars.
 *
 * The position is the bottom centre of the sprite; width and height are
 * world units. The variant picks a colour or a piece of art.
 */

#ifndef GAME_SRC_ROAD_BILLBOARD_H_
#define GAME_SRC_ROAD_BILLBOARD_H_

#include "vec3.h"

typedef enum {
  BILLBOARD_TREE,
  BILLBOARD_BUSH,
  BILLBOARD_SIGN,
  BILLBOARD_CAR,
  BILLBOARD_KINDS
} billboard_kind_t;

typedef struct {
  vec3_t position;
  double width;
  double height;
  billboard_kind_t kind;
  int variant;
} billboard_t;

#endif  // GAME_SRC_ROAD_BILLBOARD_H_
