#include "lap_timer.h"

#include <math.h>
#include <stdio.h>

lap_timer_t create_lap_timer(void) {
  lap_timer_t timer = {0, 0.0, 0.0, 0.0, false};
  return timer;
}

void update_lap_timer(lap_timer_t* timer, double seconds,
                      double previous_position, double position,
                      double track_length) {
  timer->current += seconds;
  bool crossed_forwards =
      previous_position > track_length / 2.0 && position < track_length / 2.0;
  if (!crossed_forwards) {
    return;
  }
  ++timer->laps;
  timer->last = timer->current;
  if (!timer->has_best || timer->current < timer->best) {
    timer->best = timer->current;
    timer->has_best = true;
  }
  timer->current = 0.0;
}

void format_lap_time(double seconds, char* text, size_t size) {
  int tenths = (int)floor(seconds * 10.0 + 1e-9);
  int minutes = tenths / 600;
  int whole_seconds = (tenths / 10) % 60;
  snprintf(text, size, "%d:%02d.%d", minutes, whole_seconds, tenths % 10);
}
