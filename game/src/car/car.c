#include "car.h"

#include <math.h>

#include "game_constants.h"

car_t create_car(void) {
  car_t car = {0.0, 0.0, 0.0, 0.0, 0.0};
  return car;
}

bool is_car_off_road(const car_t* car) { return fabs(car->x) > 1.0; }

static double clamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

static double wrap_position(double position, double track_length) {
  double wrapped = fmod(position, track_length);
  return wrapped < 0.0 ? wrapped + track_length : wrapped;
}

static double longitudinal_acceleration(const car_t* car, car_input_t input,
                                        road_conditions_t road) {
  double drag = CAR_DRAG_COEFFICIENT * car->speed * car->speed;
  double gravity = -CAR_GRAVITY * road.slope;
  double acceleration = gravity - drag;
  if (road.airborne) {
    return acceleration;
  }
  if (input.accelerate) {
    acceleration += CAR_ENGINE_ACCELERATION;
  } else if (input.brake) {
    acceleration += CAR_BRAKING_DECELERATION;
  } else {
    acceleration += CAR_STATIC_FRICTION +
                    CAR_ROLLING_RESISTANCE * car->speed / CAR_MAX_SPEED;
  }
  if (is_car_off_road(car) && car->speed > CAR_OFF_ROAD_LIMIT) {
    acceleration += CAR_OFF_ROAD_DECELERATION;
  }
  return acceleration;
}

static void change_speed(car_t* car, car_input_t input, double seconds,
                         road_conditions_t road) {
  double was_above_off_road_limit = car->speed > CAR_OFF_ROAD_LIMIT;
  car->acceleration = longitudinal_acceleration(car, input, road);
  car->speed += car->acceleration * seconds;
  if (is_car_off_road(car) && was_above_off_road_limit) {
    car->speed = fmax(car->speed, CAR_OFF_ROAD_LIMIT);
  }
  car->speed = clamp(car->speed, 0.0, CAR_SPEED_LIMIT);
}

static void steer(car_t* car, car_input_t input, double seconds,
                  road_conditions_t road) {
  double speed_fraction = car->speed / CAR_MAX_SPEED;
  double target = 0.0;
  if (input.left) {
    target -= CAR_STEERING_RATE * speed_fraction;
  }
  if (input.right) {
    target += CAR_STEERING_RATE * speed_fraction;
  }
  target -= road.curve * speed_fraction * speed_fraction * CAR_CENTRIFUGAL;
  double grip = road.airborne          ? 0.0
                : is_car_off_road(car) ? CAR_GRIP_ON_GRASS
                                       : CAR_GRIP_ON_ROAD;
  car->lateral_speed +=
      (target - car->lateral_speed) * fmin(1.0, grip * seconds);
  car->x += car->lateral_speed * seconds;
  if (fabs(car->x) >= CAR_MAX_X) {
    car->x = clamp(car->x, -CAR_MAX_X, CAR_MAX_X);
    car->lateral_speed = 0.0;
  }
}

void update_car(car_t* car, car_input_t input, double seconds,
                double track_length, road_conditions_t road) {
  if (!isfinite(seconds) || seconds <= 0.0) {
    return;
  }
  change_speed(car, input, seconds, road);
  car->position =
      wrap_position(car->position + car->speed * seconds, track_length);
  steer(car, input, seconds, road);
}
