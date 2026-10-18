#include "car.h"

#include <math.h>

#include "engine_note.h"
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
  double direction = car->speed < 0.0 ? -1.0 : 1.0;
  double drag = CAR_DRAG_COEFFICIENT * car->speed * fabs(car->speed);
  double gravity = -CAR_GRAVITY * road.slope;
  double acceleration = gravity - drag;
  if (road.airborne) {
    return acceleration;
  }
  double rpm = engine_rpm_fraction(car->speed);
  if (input.accelerate) {
    acceleration +=
        CAR_ENGINE_ACCELERATION * engine_pull(engine_gear(car->speed), rpm);
  } else if (input.brake || (input.reverse && car->speed > 0.0)) {
    acceleration += CAR_BRAKING_DECELERATION;
  } else if (input.reverse) {
    acceleration -= CAR_REVERSE_ACCELERATION;
  } else {
    acceleration +=
        (CAR_STATIC_FRICTION - CAR_ENGINE_BRAKING * rpm) * direction +
        CAR_ROLLING_RESISTANCE * car->speed / CAR_MAX_SPEED;
  }
  if (is_car_off_road(car) && fabs(car->speed) > CAR_OFF_ROAD_LIMIT) {
    acceleration += CAR_OFF_ROAD_DECELERATION * direction;
  }
  return acceleration;
}

static void change_speed(car_t* car, car_input_t input, double seconds,
                         road_conditions_t road) {
  double was_above_off_road_limit = car->speed > CAR_OFF_ROAD_LIMIT;
  bool was_reversing = car->speed < 0.0;
  car->acceleration = longitudinal_acceleration(car, input, road);
  car->speed += car->acceleration * seconds;
  if (is_car_off_road(car) && was_above_off_road_limit) {
    car->speed = fmax(car->speed, CAR_OFF_ROAD_LIMIT);
  }
  /* Only reverse gear takes the car below a standstill. */
  double slowest = input.reverse || was_reversing ? -CAR_REVERSE_SPEED : 0.0;
  car->speed = clamp(car->speed, slowest, CAR_SPEED_LIMIT);
}

static void steer(car_t* car, car_input_t input, double seconds,
                  road_conditions_t road) {
  double speed_fraction = fabs(car->speed) / CAR_MAX_SPEED;
  double lock = (input.right ? 1.0 : 0.0) - (input.left ? 1.0 : 0.0);
  /* Full lock moves the car less per unit of speed as speed rises. */
  double target = lock * CAR_STEERING_RATE * speed_fraction /
                  (1.0 + CAR_STEERING_WEIGHT * speed_fraction);
  target -= road.curve * speed_fraction * speed_fraction * CAR_CENTRIFUGAL;
  bool off_road = is_car_off_road(car);
  double grip = road.airborne ? 0.0
                : off_road    ? CAR_GRIP_ON_GRASS
                              : CAR_GRIP_ON_ROAD;
  /* Braking or accelerating hard shifts weight off the tyres' sideways
   * work, so the car answers the wheel more slowly; drag and friction do
   * not count, only what the driver asks for. */
  double driving = input.accelerate
                       ? CAR_ENGINE_ACCELERATION *
                             engine_pull(engine_gear(car->speed),
                                         engine_rpm_fraction(car->speed))
                   : input.brake || input.reverse ? -CAR_BRAKING_DECELERATION
                                                  : 0.0;
  double demand = clamp(driving / CAR_LONGITUDINAL_LIMIT, 0.0, 1.0);
  double most = (off_road ? CAR_LATERAL_GRIP_ON_GRASS : CAR_LATERAL_GRIP) *
                (1.0 - CAR_WEIGHT_TRANSFER * demand) * seconds;
  double change = (target - car->lateral_speed) * fmin(1.0, grip * seconds);
  car->lateral_speed += clamp(change, -most, most);
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
