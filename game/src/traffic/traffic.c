#include "traffic.h"

#include <math.h>
#include <string.h>

#include "game_constants.h"
#include "scenery_sprite.h"

#define LANES 3
#define JITTER_FRACTION 0.4

typedef struct {
  unsigned state;
} dice_t;

static unsigned roll(dice_t* dice, unsigned sides) {
  dice->state = dice->state * 1664525u + 1013904223u;
  return (dice->state >> 8) % sides;
}

static double roll_fraction(dice_t* dice) { return roll(dice, 10000) / 9999.0; }

static double lane_lateral(int lane) {
  return (lane - (LANES - 1) / 2.0) * 2.0 / LANES;
}

traffic_t create_traffic(unsigned seed, double track_length, int count) {
  traffic_t traffic;
  memset(&traffic, 0, sizeof traffic);
  dice_t dice = {seed};
  if (count > MAX_TRAFFIC) {
    count = MAX_TRAFFIC;
  }
  double spacing = track_length / count;
  for (int i = 0; i < count; ++i) {
    traffic_car_t* car = &traffic.cars[i];
    car->position =
        spacing * (i + 0.5 + JITTER_FRACTION * (roll_fraction(&dice) - 0.5));
    car->lateral = lane_lateral((int)roll(&dice, LANES));
    car->truck = i % TRAFFIC_TRUCK_EVERY == TRAFFIC_TRUCK_EVERY - 1;
    double slowest = car->truck ? TRAFFIC_TRUCK_MIN_SPEED : TRAFFIC_MIN_SPEED;
    double fastest = car->truck ? TRAFFIC_TRUCK_MAX_SPEED : TRAFFIC_MAX_SPEED;
    car->speed = slowest + (fastest - slowest) * roll_fraction(&dice);
    car->colour = (int)roll(&dice, BILLBOARD_VARIANTS);
  }
  traffic.count = count;
  return traffic;
}

static double wrap(double position, double length) {
  double wrapped = fmod(position, length);
  return wrapped < 0.0 ? wrapped + length : wrapped;
}

/* Distance from one car forward to another along the lap. */
static double gap_ahead(double from, double to, double length) {
  return wrap(to - from, length);
}

double traffic_vehicle_length(const traffic_car_t* car) {
  return car->truck ? TRAFFIC_TRUCK_LENGTH : TRAFFIC_CAR_LENGTH;
}

double traffic_vehicle_width(const traffic_car_t* car) {
  return car->truck ? TRAFFIC_TRUCK_WIDTH : TRAFFIC_CAR_WIDTH;
}

static bool same_lane(double a, double b, double width) {
  return fabs(a - b) < width + CAR_HALF_WIDTH;
}

static double speed_of_car_ahead(const traffic_t* traffic, int index,
                                 double track_length, const car_t* player,
                                 double* gap) {
  const traffic_car_t* car = &traffic->cars[index];
  double nearest = TRAFFIC_FOLLOW_DISTANCE;
  double speed = car->speed;
  for (int i = 0; i < traffic->count; ++i) {
    const traffic_car_t* other = &traffic->cars[i];
    if (i == index || !same_lane(car->lateral, other->lateral,
                                 traffic_vehicle_width(other))) {
      continue;
    }
    double distance = gap_ahead(car->position, other->position, track_length);
    if (distance < nearest) {
      nearest = distance;
      speed = other->speed;
    }
  }
  if (player && same_lane(car->lateral, player->x, CAR_HALF_WIDTH)) {
    double distance = gap_ahead(car->position, player->position, track_length);
    if (distance < nearest) {
      nearest = distance;
      speed = player->speed;
    }
  }
  *gap = nearest;
  return speed;
}

void update_traffic(traffic_t* traffic, double seconds, double track_length,
                    const car_t* player) {
  double cruise[MAX_TRAFFIC];
  for (int i = 0; i < traffic->count; ++i) {
    double gap;
    double ahead = speed_of_car_ahead(traffic, i, track_length, player, &gap);
    cruise[i] = gap < TRAFFIC_FOLLOW_DISTANCE
                    ? fmin(ahead, traffic->cars[i].speed)
                    : traffic->cars[i].speed;
  }
  for (int i = 0; i < traffic->count; ++i) {
    traffic_car_t* car = &traffic->cars[i];
    double rate =
        cruise[i] < car->speed ? TRAFFIC_BRAKE_RATE : TRAFFIC_ACCELERATION_RATE;
    car->speed += (cruise[i] - car->speed) * fmin(1.0, rate * seconds);
    car->position = wrap(car->position + car->speed * seconds, track_length);
  }
}

bool hit_traffic(const traffic_t* traffic, car_t* player, double track_length) {
  for (int i = 0; i < traffic->count; ++i) {
    const traffic_car_t* car = &traffic->cars[i];
    double ahead = gap_ahead(player->position, car->position, track_length);
    double length = traffic_vehicle_length(car);
    if (ahead > length) {
      continue;
    }
    if (fabs(player->x - car->lateral) >=
        traffic_vehicle_width(car) / 2.0 + CAR_HALF_WIDTH) {
      continue;
    }
    player->speed = fmin(player->speed, car->speed);
    player->position = wrap(car->position - length, track_length);
    player->lateral_speed =
        player->x >= car->lateral ? TRAFFIC_SHOVE : -TRAFFIC_SHOVE;
    return true;
  }
  return false;
}

billboard_t traffic_billboard(const track_t* track, const traffic_car_t* car) {
  billboard_t billboard;
  billboard_kind_t kind = car->truck ? BILLBOARD_TRUCK : BILLBOARD_CAR;
  sprite_size_t size = billboard_sprite_size(kind, car->colour);
  billboard.position = track_point_at(track, car->position, car->lateral);
  billboard.width = traffic_vehicle_width(car) * ROAD_HALF_WIDTH;
  billboard.height = billboard.width * size.height / size.width;
  billboard.kind = kind;
  billboard.variant = car->colour;
  return billboard;
}
