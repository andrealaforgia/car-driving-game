/**
 * @file road_renderer.h
 * @brief Draws the backdrop and the sorted road view with SDL.
 *
 * The road surface is textured with procedurally generated asphalt and
 * everything fades into the haze with distance. Without a renderer (as in
 * the tests) nothing is drawn.
 */

#ifndef GAME_SRC_ROAD_ROAD_RENDERER_H_
#define GAME_SRC_ROAD_ROAD_RENDERER_H_

#include <SDL.h>

#include "graphics_context.h"
#include "road_view.h"
#include "scenery_sprite.h"
#include "texture.h"

typedef struct {
  SDL_Texture* asphalt;
  texture_t mountains;
  texture_t sprites[BILLBOARD_KINDS][BILLBOARD_VARIANTS];
} road_renderer_t;

road_renderer_t init_road_renderer(const graphics_context_ptr graphics_context);
void destroy_road_renderer(road_renderer_t* road_renderer);

/* Sky, distant hills and the ground, down to the view's horizon. */
/* A rectangle of the screen to draw a view into, in pixels. */
typedef struct {
  int x, y, width, height;
} view_area_t;

view_area_t whole_screen(const graphics_context_ptr graphics_context);

void render_backdrop_in(const graphics_context_ptr graphics_context,
                        const road_renderer_t* road_renderer,
                        const road_view_t* view, view_area_t area);
void render_road_view_in(const graphics_context_ptr graphics_context,
                         const road_renderer_t* road_renderer,
                         const road_view_t* view, view_area_t area);

void render_backdrop(const graphics_context_ptr graphics_context,
                     const road_renderer_t* road_renderer,
                     const road_view_t* view);
void render_road_view(const graphics_context_ptr graphics_context,
                      const road_renderer_t* road_renderer,
                      const road_view_t* view);

#endif  // GAME_SRC_ROAD_ROAD_RENDERER_H_
