/**
 * @file dashboard.h
 * @brief What the dashboard shows: needle angles for the speedometer and
 * the rev counter, the steering wheel's rotation, and how the whole thing
 * moves with the body. Pure: no SDL. Angles are radians.
 */

#ifndef GAME_SRC_CAR_DASHBOARD_H_
#define GAME_SRC_CAR_DASHBOARD_H_

#include <stddef.h>

#include "car.h"
#include "car_body.h"
#include "engine_note.h"
#include "geometry.h" /* M_PI on every libc */

/* Needles sweep 240 degrees from rest; the wheel turns up to 108. */
#define DIAL_SWEEP (M_PI * 4.0 / 3.0)
#define WHEEL_MAX_ANGLE (M_PI * 0.6)

/* Screen angle (y down, so angles grow clockwise) of a point along a
 * dial's sweep: 0 is bottom left, 0.5 straight up, 1 bottom right. */
double dial_angle(double fraction);

typedef struct {
  double speed_needle; /* 0 at rest, DIAL_SWEEP at top speed */
  double rev_needle;   /* 0 at idle, DIAL_SWEEP at the red line */
  double wheel_angle;  /* positive turns clockwise, steering right */
  double bob;          /* pixels at 900 lines, positive moves down */
  double sway;         /* pixels at 900 lines, positive moves right */
  int gear;            /* 1 to ENGINE_GEARS */
} dashboard_state_t;

/* The gear lever's slot in an H-pattern gate: odd gears along the top
 * row, even gears along the bottom, two gears to a column. */
#define GEAR_GATE_COLUMNS ((ENGINE_GEARS + 1) / 2)

typedef struct {
  int column; /* 0 on the left */
  int row;    /* -1 top, 1 bottom */
} gear_slot_t;

gear_slot_t gear_gate_slot(int gear);

dashboard_state_t dashboard_state(const car_t* car, const car_body_t* body);

/* The speed as the driver reads it, never below zero, and as text such
 * as "76 MPH" (the HUD font has capitals and digits only). */
double speed_in_mph(double speed);
void format_speed(double speed, char* text, size_t size);

/* Where everything sits on a screen of the given size, in pixels: a low
 * band across the bottom with a dial each side and the wheel in the
 * middle, its lower half below the screen edge as if the driver were
 * looking over it. Already moved by the state's bob and sway. */
typedef struct {
  int panel_top;
  int dial_radius;
  int dial_y;
  int speed_x;
  int rev_x;
  int wheel_x;
  int wheel_y;
  int wheel_radius;
  int gate_x; /* centre of the gear gate, right of the rev counter */
  int gate_y;
  int gate_width;
  int gate_height;
  int mirror_x; /* centre of the rear-view mirror at the top */
  int mirror_y;
  int mirror_width;
  int mirror_height;
} dashboard_layout_t;

dashboard_layout_t dashboard_layout(int width, int height,
                                    const dashboard_state_t* state);

#endif  // GAME_SRC_CAR_DASHBOARD_H_
