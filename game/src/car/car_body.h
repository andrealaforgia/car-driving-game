/**
 * @file car_body.h
 * @brief What the car's body does on top of its motion along the road:
 * flying over crests, landing, suspension travel, pitch and roll.
 *
 * Pure: no SDL. Heights are world units; suspension, pitch and roll are
 * normalised to -1..1 and only drive the drawing.
 */

#ifndef GAME_SRC_CAR_CAR_BODY_H_
#define GAME_SRC_CAR_CAR_BODY_H_

#include <stdbool.h>

#include "car.h"

typedef struct {
  double height;         /* world height of the car */
  double altitude;       /* height above the road surface */
  double vertical_speed; /* world units per second */
  bool airborne;
  double suspension; /* positive is compressed */
  double suspension_speed;
  double pitch; /* positive squats under power, negative dives */
  double roll;  /* positive leans to the right of the screen */
} car_body_t;

car_body_t create_car_body(double road_height);

void update_car_body(car_body_t* body, const car_t* car, double seconds,
                     double road_height, double road_slope);

/* A knock from outside: strength 1 is a hard landing. */
void jolt_car_body(car_body_t* body, double strength);

#endif  // GAME_SRC_CAR_CAR_BODY_H_
