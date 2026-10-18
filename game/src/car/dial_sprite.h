/**
 * @file dial_sprite.h
 * @brief Paints instrument dials into RGBA pixel buffers: a black face
 * with printed tick marks and a red zone behind a chrome bezel and a
 * glass highlight, and a red needle with a counterweight and a chrome
 * hub cap. Pure: no SDL. Numbers are lettered at draw time.
 */

#ifndef GAME_SRC_CAR_DIAL_SPRITE_H_
#define GAME_SRC_CAR_DIAL_SPRITE_H_

#include <stdint.h>

#define DIAL_SPRITE_SIZE 512
#define NEEDLE_SPRITE_WIDTH 64
#define NEEDLE_SPRITE_HEIGHT 512

/* Fractions of the dial's radius: where the tick marks sit and where the
 * red zone arc runs. The needle sprite points up; its pivot is this far
 * down the sprite, and its tip reaches NEEDLE_REACH of the radius. */
#define DIAL_TICK_RADIUS 0.86
#define DIAL_RED_ZONE_RADIUS 0.74
#define NEEDLE_PIVOT_Y 0.80
#define NEEDLE_REACH 0.78

/* majors major ticks after the zero, a minor tick between each pair; red
 * zone from that fraction of the sweep (1 or more for none). */
void render_dial_face_pixels(uint32_t* pixels, int size, int majors,
                             double red_zone_from);
void render_needle_pixels(uint32_t* pixels, int width, int height);

#endif  // GAME_SRC_CAR_DIAL_SPRITE_H_
