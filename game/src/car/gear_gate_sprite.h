/**
 * @file gear_gate_sprite.h
 * @brief Paints an open gear gate and its lever into RGBA pixel buffers:
 * a brushed aluminium plate with the H slots milled into it, and a
 * chromed lever rising from a leather boot to a round knob, lit from the
 * top left. Pure: no SDL. Slot positions are fractions of the plate.
 */

#ifndef GAME_SRC_CAR_GEAR_GATE_SPRITE_H_
#define GAME_SRC_CAR_GEAR_GATE_SPRITE_H_

#include <stdint.h>

#define GATE_SPRITE_WIDTH 512
#define GATE_SPRITE_HEIGHT 384
#define LEVER_SPRITE_WIDTH 160
#define LEVER_SPRITE_HEIGHT 320

/* The lever sprite: the knob's centre and the pivot at the bottom, as
 * fractions of the sprite's height. */
#define LEVER_KNOB_Y 0.16
#define LEVER_BASE_Y 0.95

/* Centre of a slot column (0 to GEAR_GATE_COLUMNS - 1, fractional while
 * the lever is moving) and row (-1 top, 0 bar, 1 bottom) as fractions of
 * the plate's width and height. */
double gate_slot_x(double column);
double gate_slot_y(double row);

void render_gear_gate_pixels(uint32_t* pixels, int width, int height);
void render_gear_lever_pixels(uint32_t* pixels, int width, int height);

#endif  // GAME_SRC_CAR_GEAR_GATE_SPRITE_H_
