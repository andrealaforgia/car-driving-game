#include "road_renderer.h"

#include <math.h>
#include <stdlib.h>

#include "color.h"
#include "drawing_primitives.h"
#include "game_constants.h"
#include "geometry.h"
#include "road_texture.h"

#define COLOR_SKY_TOP 0x2A66D6
#define COLOR_SKY_HORIZON 0xB4D4F2
#define COLOR_HAZE 0xB4D4F2
#define COLOR_MOUNTAIN 0x7C7AA4
#define COLOR_MOUNTAIN_SHADE 0x66648C
#define COLOR_GRASS_LIGHT 0x10AA10
#define COLOR_GRASS_DARK 0x009A00
#define COLOR_RUMBLE_LIGHT 0xFFFFFF
#define COLOR_RUMBLE_DARK 0xBB0000
#define COLOR_ROAD_LIGHT 0x6B6B6B
#define COLOR_ROAD_DARK 0x696969
#define TINT_ROAD_LIGHT 0xFFFFFF
#define TINT_ROAD_DARK 0xEEEEEE
#define COLOR_LANE 0xDDDDDD
#define ASPHALT_SEED 1974u
#define SKY_BANDS 24
#define MOUNTAIN_WIDTH 2048
#define MOUNTAIN_HEIGHT 96
#define MOUNTAIN_SEED 7u

static SDL_Texture* create_asphalt_texture(SDL_Renderer* renderer) {
  if (!renderer) {
    return NULL;
  }
  uint32_t* pixels =
      calloc(ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE, sizeof(uint32_t));
  if (!pixels) {
    return NULL;
  }
  generate_asphalt_pixels(pixels, ASPHALT_SEED);
  SDL_Texture* texture = SDL_CreateTexture(
      renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
      ROAD_TEXTURE_SIZE, ROAD_TEXTURE_SIZE);
  if (texture) {
    SDL_UpdateTexture(texture, NULL, pixels,
                      ROAD_TEXTURE_SIZE * sizeof(uint32_t));
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
  }
  free(pixels);
  return texture;
}

static texture_t upload(SDL_Renderer* renderer, const uint32_t* pixels,
                        int width, int height, SDL_ScaleMode scale_mode) {
  texture_t texture = {NULL, width, height};
  if (!renderer) {
    return texture;
  }
  texture.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STATIC, width, height);
  if (texture.texture) {
    SDL_UpdateTexture(texture.texture, NULL, pixels,
                      width * (int)sizeof(uint32_t));
    SDL_SetTextureBlendMode(texture.texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture.texture, scale_mode);
  }
  return texture;
}

static uint32_t rgba(color_t color) {
  return 0xFF000000u | ((uint32_t)B(color) << 16) | ((uint32_t)G(color) << 8) |
         (uint32_t)R(color);
}

/* A silhouette of distant hills that wraps around the whole horizon. */
static texture_t create_mountains(SDL_Renderer* renderer) {
  texture_t none = {NULL, 0, 0};
  uint32_t* pixels = calloc(MOUNTAIN_WIDTH * MOUNTAIN_HEIGHT, sizeof(uint32_t));
  if (!pixels) {
    return none;
  }
  unsigned noise = MOUNTAIN_SEED;
  for (int x = 0; x < MOUNTAIN_WIDTH; ++x) {
    double angle = 2.0 * M_PI * x / MOUNTAIN_WIDTH;
    double ridge = 34.0 + 22.0 * sin(angle * 3.0 + 1.0) +
                   14.0 * sin(angle * 7.0 + 2.5) + 9.0 * sin(angle * 13.0) +
                   5.0 * sin(angle * 29.0 + 0.7);
    noise = noise * 1664525u + 1013904223u;
    ridge += (int)((noise >> 8) % 5) - 2;
    int top = MOUNTAIN_HEIGHT - (int)ridge;
    for (int y = top; y < MOUNTAIN_HEIGHT; ++y) {
      bool lit = (x / 24 + y / 12) % 2 == 0 && y < top + 10;
      pixels[y * MOUNTAIN_WIDTH + x] =
          rgba(lit ? COLOR_MOUNTAIN : COLOR_MOUNTAIN_SHADE);
    }
  }
  texture_t texture = upload(renderer, pixels, MOUNTAIN_WIDTH, MOUNTAIN_HEIGHT,
                             SDL_ScaleModeLinear);
  free(pixels);
  return texture;
}

road_renderer_t init_road_renderer(
    const graphics_context_ptr graphics_context) {
  road_renderer_t road_renderer;
  SDL_Renderer* renderer = graphics_context->renderer;
  road_renderer.asphalt = create_asphalt_texture(renderer);
  road_renderer.mountains = create_mountains(renderer);
  uint32_t* pixels =
      calloc(BILLBOARD_MAX_WIDTH * BILLBOARD_MAX_HEIGHT, sizeof(uint32_t));
  for (int kind = 0; kind < BILLBOARD_KINDS; ++kind) {
    for (int variant = 0; variant < BILLBOARD_VARIANTS; ++variant) {
      texture_t none = {NULL, 0, 0};
      road_renderer.sprites[kind][variant] = none;
      if (!pixels || variant >= billboard_variants((billboard_kind_t)kind)) {
        continue;
      }
      sprite_size_t size =
          billboard_sprite_size((billboard_kind_t)kind, variant);
      render_billboard_pixels(pixels, (billboard_kind_t)kind, variant);
      road_renderer.sprites[kind][variant] = upload(
          renderer, pixels, size.width, size.height, SDL_ScaleModeNearest);
    }
  }
  free(pixels);
  if (renderer) {
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  }
  return road_renderer;
}

void destroy_road_renderer(road_renderer_t* road_renderer) {
  if (road_renderer->asphalt) {
    SDL_DestroyTexture(road_renderer->asphalt);
    road_renderer->asphalt = NULL;
  }
  free_texture(&road_renderer->mountains);
  for (int kind = 0; kind < BILLBOARD_KINDS; ++kind) {
    for (int variant = 0; variant < BILLBOARD_VARIANTS; ++variant) {
      free_texture(&road_renderer->sprites[kind][variant]);
    }
  }
}

static color_t mix(color_t from, color_t to, double amount) {
  int r = (int)lround(R(from) + (R(to) - R(from)) * amount);
  int g = (int)lround(G(from) + (G(to) - G(from)) * amount);
  int b = (int)lround(B(from) + (B(to) - B(from)) * amount);
  return COLOR(r, g, b);
}

/* The hills sit at infinity, so they only scroll with the camera's yaw:
 * one full turn pans across the whole texture. */
static void draw_mountains(const graphics_context_ptr graphics_context,
                           const road_renderer_t* road_renderer,
                           const road_view_t* view, view_area_t area,
                           int horizon) {
  const texture_ptr mountains = (const texture_ptr)&road_renderer->mountains;
  if (!mountains->texture) {
    return;
  }
  int width = area.width;
  int height = (int)(area.height * 0.16);
  int span = (int)(MOUNTAIN_WIDTH * CAMERA_FIELD_OF_VIEW_DEGREES / 360.0);
  double turn = fmod(view->pose.yaw / (2.0 * M_PI), 1.0);
  if (turn < 0.0) {
    turn += 1.0;
  }
  int start = ((int)(turn * MOUNTAIN_WIDTH) - span / 2 + MOUNTAIN_WIDTH) %
              MOUNTAIN_WIDTH;
  int first = span < MOUNTAIN_WIDTH - start ? span : MOUNTAIN_WIDTH - start;
  int first_width = width * first / span;
  rect_t source = make_rect(start, 0, first, MOUNTAIN_HEIGHT);
  rect_t destination = make_rect(area.x, horizon - height, first_width, height);
  render_sprite(graphics_context, mountains, &source, &destination);
  if (first < span) {
    rect_t rest = make_rect(0, 0, span - first, MOUNTAIN_HEIGHT);
    rect_t rest_destination = make_rect(area.x + first_width, horizon - height,
                                        width - first_width, height);
    render_sprite(graphics_context, mountains, &rest, &rest_destination);
  }
}

view_area_t whole_screen(const graphics_context_ptr graphics_context) {
  view_area_t area = {0, 0, graphics_context->screen_width,
                      graphics_context->screen_height};
  return area;
}

void render_backdrop_in(const graphics_context_ptr graphics_context,
                        const road_renderer_t* road_renderer,
                        const road_view_t* view, view_area_t area) {
  int horizon = area.y + (int)lround(view->horizon * area.height);
  int sky_height = horizon > area.y ? horizon - area.y : 0;
  for (int band = 0; band < SKY_BANDS; ++band) {
    int top = area.y + sky_height * band / SKY_BANDS;
    int bottom = area.y + sky_height * (band + 1) / SKY_BANDS;
    color_t color = mix(COLOR_SKY_TOP, COLOR_SKY_HORIZON,
                        pow((double)band / (SKY_BANDS - 1), 1.5));
    draw_filled_rect(graphics_context, area.x, top, area.width, bottom - top,
                     color);
  }
  draw_mountains(graphics_context, road_renderer, view, area, horizon);
  int bottom = area.y + area.height;
  if (horizon < bottom) {
    draw_filled_rect(graphics_context, area.x, horizon, area.width,
                     bottom - horizon, mix(COLOR_GRASS_DARK, COLOR_HAZE, 0.9));
  }
}

void render_backdrop(const graphics_context_ptr graphics_context,
                     const road_renderer_t* road_renderer,
                     const road_view_t* view) {
  render_backdrop_in(graphics_context, road_renderer, view,
                     whole_screen(graphics_context));
}

static color_t polygon_color(const view_polygon_t* polygon) {
  switch (polygon->surface) {
    case SURFACE_GRASS:
      return polygon->light ? COLOR_GRASS_LIGHT : COLOR_GRASS_DARK;
    case SURFACE_RUMBLE:
      return polygon->light ? COLOR_RUMBLE_LIGHT : COLOR_RUMBLE_DARK;
    case SURFACE_ROAD:
      return polygon->light ? COLOR_ROAD_LIGHT : COLOR_ROAD_DARK;
    default:
      return COLOR_LANE;
  }
}

static void fill_vertices(view_area_t area, const view_polygon_t* polygon,
                          SDL_Vertex* vertices, color_t color, uint8_t alpha) {
  SDL_Color sdl_color = {(uint8_t)R(color), (uint8_t)G(color),
                         (uint8_t)B(color), alpha};
  for (int i = 0; i < polygon->vertex_count; ++i) {
    vertices[i].position.x =
        (float)(area.x + polygon->vertices[i].x * area.width);
    vertices[i].position.y =
        (float)(area.y + polygon->vertices[i].y * area.height);
    vertices[i].color = sdl_color;
    vertices[i].tex_coord.x = (float)polygon->vertices[i].u;
    vertices[i].tex_coord.y = (float)polygon->vertices[i].v;
  }
}

static void draw_polygon(const graphics_context_ptr graphics_context,
                         const road_renderer_t* road_renderer,
                         const view_polygon_t* polygon, view_area_t area) {
  SDL_Vertex vertices[POLYGON_MAX_VERTICES];
  int indices[(POLYGON_MAX_VERTICES - 2) * 3];
  int index_count = 0;
  for (int i = 1; i + 1 < polygon->vertex_count; ++i) {
    indices[index_count++] = 0;
    indices[index_count++] = i;
    indices[index_count++] = i + 1;
  }
  bool textured = polygon->surface == SURFACE_ROAD && road_renderer->asphalt;
  if (textured) {
    fill_vertices(area, polygon, vertices,
                  polygon->light ? TINT_ROAD_LIGHT : TINT_ROAD_DARK, 255);
    SDL_RenderGeometry(graphics_context->renderer, road_renderer->asphalt,
                       vertices, polygon->vertex_count, indices, index_count);
    if (polygon->fog > 0.02) {
      fill_vertices(area, polygon, vertices, COLOR_HAZE,
                    (uint8_t)(polygon->fog * 255.0));
      SDL_RenderGeometry(graphics_context->renderer, NULL, vertices,
                         polygon->vertex_count, indices, index_count);
    }
    return;
  }
  fill_vertices(area, polygon, vertices,
                mix(polygon_color(polygon), COLOR_HAZE, polygon->fog), 255);
  SDL_RenderGeometry(graphics_context->renderer, NULL, vertices,
                     polygon->vertex_count, indices, index_count);
}

static void draw_sprite(const graphics_context_ptr graphics_context,
                        const road_renderer_t* road_renderer,
                        const view_sprite_t* sprite, view_area_t area) {
  int variant = sprite->variant % billboard_variants(sprite->kind);
  const texture_ptr texture =
      (const texture_ptr)&road_renderer->sprites[sprite->kind][variant];
  if (!texture->texture) {
    return;
  }
  int width = (int)lround(sprite->width * area.width);
  int height = (int)lround(sprite->height * area.height);
  if (width < 1 || height < 1) {
    return;
  }
  int x = area.x + (int)lround(sprite->x * area.width) - width / 2;
  int bottom = area.y + (int)lround(sprite->y * area.height);
  rect_t destination = make_rect(x, bottom - height, width, height);
  SDL_SetTextureAlphaMod(texture->texture,
                         (uint8_t)((1.0 - sprite->fog) * 255.0));
  render_sprite(graphics_context, texture, NULL, &destination);
  SDL_SetTextureAlphaMod(texture->texture, 255);
}

void render_road_view_in(const graphics_context_ptr graphics_context,
                         const road_renderer_t* road_renderer,
                         const road_view_t* view, view_area_t area) {
  if (!graphics_context->renderer) {
    return;
  }
  for (int i = 0; i < view->order_count; ++i) {
    int id = view->order[i];
    if (id < MAX_VIEW_POLYGONS) {
      draw_polygon(graphics_context, road_renderer, &view->polygons[id], area);
    } else {
      draw_sprite(graphics_context, road_renderer,
                  &view->sprites[id - MAX_VIEW_POLYGONS], area);
    }
  }
}

void render_road_view(const graphics_context_ptr graphics_context,
                      const road_renderer_t* road_renderer,
                      const road_view_t* view) {
  render_road_view_in(graphics_context, road_renderer, view,
                      whole_screen(graphics_context));
}
