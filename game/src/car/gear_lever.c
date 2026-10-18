#include "gear_lever.h"

#include <math.h>

#include "dashboard.h"

gear_lever_t create_gear_lever(int gear) {
  gear_slot_t slot = gear_gate_slot(gear);
  gear_lever_t lever = {slot.column, slot.row};
  return lever;
}

/* Moves value towards target by at most step; returns the step left. */
static double approach(double* value, double target, double step) {
  double gap = target - *value;
  if (fabs(gap) <= step) {
    *value = target;
    return step - fabs(gap);
  }
  *value += gap > 0.0 ? step : -step;
  return 0.0;
}

void update_gear_lever(gear_lever_t* lever, int gear, double seconds) {
  gear_slot_t slot = gear_gate_slot(gear);
  double step = GEAR_LEVER_SPEED * seconds;
  while (step > 0.0) {
    if (lever->column != slot.column) {
      if (lever->row != 0.0) {
        step = approach(&lever->row, 0.0, step);
      } else {
        step = approach(&lever->column, slot.column, step);
      }
    } else if (lever->row != slot.row) {
      step = approach(&lever->row, slot.row, step);
    } else {
      return;
    }
  }
}
