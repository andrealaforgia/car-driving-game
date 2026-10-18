/**
 * @file steering_wheel_sprite.h
 * @brief Paints a steering wheel seen face on into an RGBA pixel buffer:
 * a leather rim with stitching, three metal spokes and a padded hub, lit
 * from the top left, anti-aliased, transparent everywhere else. Pure: no
 * SDL. Pixels are RGBA32 (R in the lowest byte, A in the highest).
 */

#ifndef GAME_SRC_CAR_STEERING_WHEEL_SPRITE_H_
#define GAME_SRC_CAR_STEERING_WHEEL_SPRITE_H_

#include <stdint.h>

#define WHEEL_SPRITE_SIZE 1024

/* pixels holds size * size entries; the wheel fills the square. */
void render_steering_wheel_pixels(uint32_t* pixels, int size);

#endif  // GAME_SRC_CAR_STEERING_WHEEL_SPRITE_H_
