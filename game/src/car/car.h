/**
 * @file car.h
 * @brief The player's car: distance along the track, speed and lateral
 * position. The lateral position is normalised so -1 and 1 are the road
 * edges.
 *
 * Speed is limited by air drag, so the top speed on the flat emerges from
 * engine force against drag; gravity along the slope adds or removes
 * speed on hills. Steering sets a target lateral speed that the car reaches
 * through its grip, which is weaker on grass.
 */

#ifndef GAME_SRC_CAR_CAR_H_
#define GAME_SRC_CAR_CAR_H_

#include <stdbool.h>

typedef struct {
  bool accelerate;
  bool brake;
  bool left;
  bool right;
  bool reverse; /* brakes while moving forward, then backs up */
} car_input_t;

typedef struct {
  double curve;
  double slope;
  bool airborne;
} road_conditions_t;

typedef struct {
  double position;
  double speed;
  double x;
  double lateral_speed;
  double acceleration;
} car_t;

car_t create_car(void);

void update_car(car_t* car, car_input_t input, double seconds,
                double track_length, road_conditions_t road);

bool is_car_off_road(const car_t* car);

#endif  // GAME_SRC_CAR_CAR_H_
