#include "car_body.h"

#include <math.h>

#include "game_constants.h"
#include "geometry.h"

car_body_t create_car_body(double road_height) {
  car_body_t body = {road_height, 0.0, 0.0, false, 0.0, 0.0, 0.0, 0.0};
  return body;
}

static double clamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

static double approach(double value, double target, double rate,
                       double seconds) {
  return value + (target - value) * fmin(1.0, rate * seconds);
}

/* The road drops away faster than gravity can follow, and the hop would be
 * too big for the suspension to absorb. */
static bool road_falls_away(const car_body_t* body, double road_vertical_speed,
                            double seconds) {
  double required_deceleration =
      (body->vertical_speed - road_vertical_speed) / seconds;
  double hop_altitude =
      body->vertical_speed * body->vertical_speed / (2.0 * BODY_GRAVITY);
  return required_deceleration > BODY_GRAVITY &&
         hop_altitude > BODY_MIN_HOP_ALTITUDE;
}

static void fly(car_body_t* body, double seconds) {
  body->vertical_speed -= BODY_GRAVITY * seconds;
  body->height += body->vertical_speed * seconds;
}

static void land(car_body_t* body, double road_height,
                 double road_vertical_speed) {
  double impact = road_vertical_speed - body->vertical_speed;
  body->suspension_speed += impact / CAR_MAX_SPEED * LANDING_KICK;
  body->height = road_height;
  body->vertical_speed = road_vertical_speed;
  body->airborne = false;
}

static void follow_the_road(car_body_t* body, const car_t* car, double seconds,
                            double road_height, double road_slope) {
  double road_vertical_speed = road_slope * car->speed;
  if (body->airborne) {
    fly(body, seconds);
    if (body->height <= road_height) {
      land(body, road_height, road_vertical_speed);
    }
  } else if (road_falls_away(body, road_vertical_speed, seconds)) {
    body->airborne = true;
    fly(body, seconds);
  } else {
    body->height = road_height;
    body->vertical_speed = road_vertical_speed;
  }
  body->altitude = fmax(0.0, body->height - road_height);
}

static double grass_bump_force(const car_t* car) {
  if (!is_car_off_road(car)) {
    return 0.0;
  }
  double phase = car->position / GRASS_BUMP_LENGTH * 2.0 * M_PI;
  return GRASS_SHAKE_FORCE * sin(phase) * car->speed / CAR_MAX_SPEED;
}

static void settle_the_suspension(car_body_t* body, const car_t* car,
                                  double seconds) {
  double force = -SUSPENSION_STIFFNESS * body->suspension -
                 SUSPENSION_DAMPING * body->suspension_speed +
                 grass_bump_force(car);
  body->suspension_speed += force * seconds;
  body->suspension += body->suspension_speed * seconds;
  if (fabs(body->suspension) > SUSPENSION_MAX_TRAVEL) {
    body->suspension =
        clamp(body->suspension, -SUSPENSION_MAX_TRAVEL, SUSPENSION_MAX_TRAVEL);
    body->suspension_speed = 0.0;
  }
}

void update_car_body(car_body_t* body, const car_t* car, double seconds,
                     double road_height, double road_slope) {
  follow_the_road(body, car, seconds, road_height, road_slope);
  settle_the_suspension(body, car, seconds);
  double pitch_target =
      clamp(car->acceleration / CAR_ENGINE_ACCELERATION, -1.0, 1.0);
  double roll_target =
      clamp(-car->lateral_speed / CAR_STEERING_RATE, -1.0, 1.0);
  body->pitch = approach(body->pitch, pitch_target, PITCH_RESPONSE, seconds);
  body->roll = approach(body->roll, roll_target, ROLL_RESPONSE, seconds);
}

void jolt_car_body(car_body_t* body, double strength) {
  body->suspension_speed += strength * LANDING_KICK;
}
