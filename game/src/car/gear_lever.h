/**
 * @file gear_lever.h
 * @brief Where the gear lever is in the H gate, sliding from slot to slot
 * the way a real lever moves: out to the neutral bar, across, and into
 * the new slot. Pure: no SDL. Positions are in slots: column 0 on the
 * left, row -1 at the top, 0 on the bar, 1 at the bottom.
 */

#ifndef GAME_SRC_CAR_GEAR_LEVER_H_
#define GAME_SRC_CAR_GEAR_LEVER_H_

/* Slots per second. */
#define GEAR_LEVER_SPEED 12.0

typedef struct {
  double column;
  double row;
} gear_lever_t;

gear_lever_t create_gear_lever(int gear);
void update_gear_lever(gear_lever_t* lever, int gear, double seconds);

#endif  // GAME_SRC_CAR_GEAR_LEVER_H_
