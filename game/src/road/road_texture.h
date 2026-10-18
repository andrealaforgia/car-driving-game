/**
 * @file road_texture.h
 * @brief Procedural asphalt: grey noise generated at startup, no asset files.
 *
 * Pixels are RGBA32 (R in the low byte), which is what SDL_PIXELFORMAT_RGBA32
 * expects. Pure: no SDL here.
 */

#ifndef GAME_SRC_ROAD_ROAD_TEXTURE_H_
#define GAME_SRC_ROAD_ROAD_TEXTURE_H_

#include <stdint.h>

#define ROAD_TEXTURE_SIZE 256
#define ROAD_TEXTURE_BASE_GREY 0x6A
#define ROAD_TEXTURE_MAX_DEVIATION 14
/* Road segments covered by one repeat of the texture along the road. */
#define ROAD_TEXTURE_SEGMENTS 4

void generate_asphalt_pixels(uint32_t* pixels, unsigned seed);

#endif  // GAME_SRC_ROAD_ROAD_TEXTURE_H_
