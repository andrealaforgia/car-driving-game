#include "dashboard.h"

#include <math.h>
#include <stdio.h>

#include "engine_note.h"
#include "game_constants.h"
#include "geometry.h"

#define BOB_PIXELS 14.0
#define PITCH_PIXELS 5.0
#define SWAY_PIXELS 9.0

#define PANEL_TOP_FRACTION 0.78
#define DIAL_RADIUS_FRACTION 0.09
#define DIAL_DROP_FRACTION 0.115
#define SPEED_X_FRACTION 0.27
#define REV_X_FRACTION 0.73
#define WHEEL_RADIUS_FRACTION 0.24
#define WHEEL_DROP_FRACTION 0.19

static double clamp(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}

dashboard_state_t dashboard_state(const car_t* car, const car_body_t* body) {
  dashboard_state_t state;
  state.speed_needle = clamp(car->speed / CAR_MAX_SPEED, 0.0, 1.0) * DIAL_SWEEP;
  state.rev_needle = engine_rpm_fraction(car->speed) * DIAL_SWEEP;
  state.wheel_angle = clamp(car->lateral_speed / CAR_STEERING_RATE, -1.0, 1.0) *
                      WHEEL_MAX_ANGLE;
  state.bob = body->suspension * BOB_PIXELS - body->pitch * PITCH_PIXELS;
  state.sway = body->roll * SWAY_PIXELS;
  return state;
}

dashboard_layout_t dashboard_layout(int width, int height,
                                    const dashboard_state_t* state) {
  double unit = height / 900.0;
  int bob = (int)lround(state->bob * unit);
  int sway = (int)lround(state->sway * unit);
  dashboard_layout_t layout;
  layout.panel_top = (int)lround(height * PANEL_TOP_FRACTION) + bob;
  layout.dial_radius = (int)lround(height * DIAL_RADIUS_FRACTION);
  layout.dial_y = layout.panel_top + (int)lround(height * DIAL_DROP_FRACTION);
  layout.speed_x = (int)lround(width * SPEED_X_FRACTION) + sway;
  layout.rev_x = (int)lround(width * REV_X_FRACTION) + sway;
  layout.wheel_x = width / 2 + sway;
  layout.wheel_y = layout.panel_top + (int)lround(height * WHEEL_DROP_FRACTION);
  layout.wheel_radius = (int)lround(height * WHEEL_RADIUS_FRACTION);
  return layout;
}

double speed_in_mph(double speed) {
  return speed < 0.0 ? 0.0 : speed / CAR_MAX_SPEED * CAR_TOP_SPEED_MPH;
}

void format_speed(double speed, char* text, size_t size) {
  snprintf(text, size, "%d MPH", (int)lround(speed_in_mph(speed)));
}
