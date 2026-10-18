/**
 * @file scenery_sprite.h
 * @brief Pixel art for the things standing beside the road and the traffic,
 * authored as character grids and rendered to RGBA pixels.
 */

#ifndef GAME_SRC_ROAD_SCENERY_SPRITE_H_
#define GAME_SRC_ROAD_SCENERY_SPRITE_H_

#include <stdbool.h>
#include <stdint.h>

#include "billboard.h"

#define BILLBOARD_VARIANTS 6
#define BILLBOARD_MAX_WIDTH 32
#define BILLBOARD_MAX_HEIGHT 40

typedef struct {
  int width;
  int height;
} sprite_size_t;

/* The number of distinct pieces of art for a kind; variants beyond it
 * reuse the art (cars reuse it in different colours). */
int billboard_variants(billboard_kind_t kind);

sprite_size_t billboard_sprite_size(billboard_kind_t kind, int variant);

bool billboard_art_is_well_formed(void);

/* pixels must hold width * height RGBA32 values. */
void render_billboard_pixels(uint32_t* pixels, billboard_kind_t kind,
                             int variant);

#endif  // GAME_SRC_ROAD_SCENERY_SPRITE_H_
