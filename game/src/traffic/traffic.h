/**
 * @file traffic.h
 * @brief Other cars on the road: they cruise in their lanes, slow down
 * behind whoever is ahead, and stop the player when run into.
 *
 * Pure: no SDL. Positions are distances along the track like the player's;
 * lateral is in road half widths.
 */

#ifndef GAME_SRC_TRAFFIC_TRAFFIC_H_
#define GAME_SRC_TRAFFIC_TRAFFIC_H_

#include <stdbool.h>

#include "billboard.h"
#include "car.h"
#include "track.h"

#define MAX_TRAFFIC 24

typedef struct {
  double position;
  double lateral;
  double speed;
  int colour;
} traffic_car_t;

typedef struct {
  traffic_car_t cars[MAX_TRAFFIC];
  int count;
} traffic_t;

traffic_t create_traffic(unsigned seed, double track_length, int count);

/* player may be NULL; traffic slows down behind it like behind a car. */
void update_traffic(traffic_t* traffic, double seconds, double track_length,
                    const car_t* player);

/* Resolves a collision with the player: slows the player to the car's
 * speed, keeps it behind and shoves it sideways. True when it happened. */
bool hit_traffic(const traffic_t* traffic, car_t* player, double track_length);

billboard_t traffic_billboard(const track_t* track, const traffic_car_t* car);

#endif  // GAME_SRC_TRAFFIC_TRAFFIC_H_
