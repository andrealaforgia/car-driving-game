/**
 * @file lap_timer.h
 * @brief Counts laps as the car crosses the start line going forwards,
 * and keeps the current, last and best lap times.
 */

#ifndef GAME_SRC_CAR_LAP_TIMER_H_
#define GAME_SRC_CAR_LAP_TIMER_H_

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  int laps;
  double current;
  double last;
  double best;
  bool has_best;
} lap_timer_t;

lap_timer_t create_lap_timer(void);

void update_lap_timer(lap_timer_t* timer, double seconds,
                      double previous_position, double position,
                      double track_length);

/* "m:ss.t" */
void format_lap_time(double seconds, char* text, size_t size);

#endif  // GAME_SRC_CAR_LAP_TIMER_H_
