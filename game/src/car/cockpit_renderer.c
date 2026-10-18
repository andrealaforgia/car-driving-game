#include "cockpit_renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dashboard.h"
#include "drawing_primitives.h"
#include "game_constants.h"
#include "geometry.h"
#include "steering_wheel_sprite.h"
#include "text.h"

#define COLOR_PANEL 0x1A1A1E
#define COLOR_PANEL_EDGE 0x55555E
#define COLOR_DIAL 0x0C0C10
#define COLOR_BEZEL 0xB8B8C0
#define COLOR_TICK 0xE8E8E8
#define COLOR_RED_ZONE 0xD02020
#define COLOR_NEEDLE 0xFF4030
#define COLOR_HUB 0x8A8A92
#define COLOR_LABEL 0xC0C0C8

/* The road shows through the band and the dial faces. */
#define PANEL_ALPHA 96
#define DIAL_ALPHA 170

/* Dials start at 7 o'clock and sweep clockwise. */
#define DIAL_START (M_PI * 7.0 / 6.0)
#define TICKS 12
#define RED_ZONE_FROM 0.85
#define RING_SIDES 24

cockpit_renderer_t init_cockpit_renderer(
    const graphics_context_ptr graphics_context) {
  cockpit_renderer_t renderer;
  renderer.wheel = (texture_t){NULL, WHEEL_SPRITE_SIZE, WHEEL_SPRITE_SIZE};
  if (!graphics_context->renderer) {
    return renderer;
  }
  uint32_t* pixels =
      calloc((size_t)WHEEL_SPRITE_SIZE * WHEEL_SPRITE_SIZE, sizeof(uint32_t));
  if (!pixels) {
    return renderer;
  }
  render_steering_wheel_pixels(pixels, WHEEL_SPRITE_SIZE);
  renderer.wheel.texture = SDL_CreateTexture(
      graphics_context->renderer, SDL_PIXELFORMAT_RGBA32,
      SDL_TEXTUREACCESS_STATIC, WHEEL_SPRITE_SIZE, WHEEL_SPRITE_SIZE);
  if (renderer.wheel.texture) {
    SDL_UpdateTexture(renderer.wheel.texture, NULL, pixels,
                      WHEEL_SPRITE_SIZE * (int)sizeof(uint32_t));
    SDL_SetTextureBlendMode(renderer.wheel.texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(renderer.wheel.texture, SDL_ScaleModeLinear);
  }
  free(pixels);
  return renderer;
}

void destroy_cockpit_renderer(cockpit_renderer_t* renderer) {
  free_texture(&renderer->wheel);
}

/* A translucent convex polygon: a fan of blended triangles. */
static void draw_translucent_polygon(
    const graphics_context_ptr graphics_context, const SDL_Point* points,
    int count, color_t color, uint8_t alpha) {
  SDL_SetRenderDrawBlendMode(graphics_context->renderer, SDL_BLENDMODE_BLEND);
  SDL_Color tint = {R(color), G(color), B(color), alpha};
  SDL_Vertex vertices[3];
  vertices[0].position.x = (float)points[0].x;
  vertices[0].position.y = (float)points[0].y;
  for (int i = 0; i < 3; ++i) {
    vertices[i].color = tint;
    vertices[i].tex_coord = (SDL_FPoint){0.0f, 0.0f};
  }
  for (int i = 1; i + 1 < count; ++i) {
    vertices[1].position.x = (float)points[i].x;
    vertices[1].position.y = (float)points[i].y;
    vertices[2].position.x = (float)points[i + 1].x;
    vertices[2].position.y = (float)points[i + 1].y;
    SDL_RenderGeometry(graphics_context->renderer, NULL, vertices, 3, NULL, 0);
  }
}

static void draw_panel(const graphics_context_ptr graphics_context,
                       const dashboard_layout_t* layout, int width,
                       int height) {
  int top = layout->panel_top;
  int s = layout->wheel_x - width / 2;
  int lip = (height * 3) / 100;
  SDL_Point panel[6] = {
      {-width / 10 + s, top + lip}, {width / 8 + s, top},
      {width * 7 / 8 + s, top},     {width + width / 10 + s, top + lip},
      {width + width / 10, height}, {-width / 10, height}};
  draw_translucent_polygon(graphics_context, panel, 6, COLOR_PANEL,
                           PANEL_ALPHA);
  draw_line(graphics_context, width / 8 + s, top, width * 7 / 8 + s, top,
            COLOR_PANEL_EDGE);
}

/* A ring as a band of quads, so it needs no circle lookup table. */
static void draw_ring(const graphics_context_ptr graphics_context, int cx,
                      int cy, int radius, int thickness, color_t color) {
  double outer = radius + thickness / 2.0;
  double inner = radius - thickness / 2.0;
  for (int i = 0; i < RING_SIDES * 2; ++i) {
    double a = 2.0 * M_PI * i / (RING_SIDES * 2);
    double b = 2.0 * M_PI * (i + 1) / (RING_SIDES * 2);
    SDL_Point band[4] = {
        {cx + (int)lround(outer * cos(a)), cy + (int)lround(outer * sin(a))},
        {cx + (int)lround(outer * cos(b)), cy + (int)lround(outer * sin(b))},
        {cx + (int)lround(inner * cos(b)), cy + (int)lround(inner * sin(b))},
        {cx + (int)lround(inner * cos(a)), cy + (int)lround(inner * sin(a))}};
    draw_filled_polygon(graphics_context, band, 4, color);
  }
}

static void disc_points(SDL_Point* points, int cx, int cy, int radius) {
  for (int i = 0; i < RING_SIDES; ++i) {
    double angle = 2.0 * M_PI * i / RING_SIDES;
    points[i].x = cx + (int)lround(radius * cos(angle));
    points[i].y = cy + (int)lround(radius * sin(angle));
  }
}

static void draw_disc(const graphics_context_ptr graphics_context, int cx,
                      int cy, int radius, color_t color) {
  SDL_Point points[RING_SIDES];
  disc_points(points, cx, cy, radius);
  draw_filled_polygon(graphics_context, points, RING_SIDES, color);
}

/* Angle on screen for a position along a dial's sweep (0 to 1). */
static double dial_angle(double fraction) {
  return DIAL_START + fraction * DIAL_SWEEP;
}

static void draw_dial(const graphics_context_ptr graphics_context,
                      const dashboard_layout_t* layout, int cx, int cy,
                      double needle, bool red_zone, const char* label) {
  int r = layout->dial_radius;
  SDL_Point face[RING_SIDES];
  disc_points(face, cx, cy, r);
  draw_translucent_polygon(graphics_context, face, RING_SIDES, COLOR_DIAL,
                           DIAL_ALPHA);
  draw_ring(graphics_context, cx, cy, r, 4, COLOR_BEZEL);
  for (int tick = 0; tick <= TICKS; ++tick) {
    double fraction = (double)tick / TICKS;
    double angle = dial_angle(fraction);
    bool major = tick % 2 == 0;
    int inner = r - (major ? r / 5 : r / 9);
    color_t color =
        red_zone && fraction >= RED_ZONE_FROM ? COLOR_RED_ZONE : COLOR_TICK;
    draw_thick_line(graphics_context, cx + (int)(inner * cos(angle)),
                    cy + (int)(inner * sin(angle)),
                    cx + (int)((r - 4) * cos(angle)),
                    cy + (int)((r - 4) * sin(angle)), color);
  }
  if (red_zone) {
    int arc = r - r / 7;
    for (double f = RED_ZONE_FROM; f < 1.0; f += 0.01) {
      double a = dial_angle(f), b = dial_angle(f + 0.01);
      draw_thick_line(graphics_context, cx + (int)(arc * cos(a)),
                      cy + (int)(arc * sin(a)), cx + (int)(arc * cos(b)),
                      cy + (int)(arc * sin(b)), COLOR_RED_ZONE);
    }
  }
  int scale = r / 36 > 0 ? r / 36 : 1;
  text_dimensions_t size = calculate_text_dimensions(label, scale);
  write_text(graphics_context, label, point(cx - size.width / 2, cy + r / 2),
             scale, COLOR_LABEL);
  double angle = DIAL_START + needle;
  int length = r - r / 6;
  draw_thick_line(graphics_context, cx, cy, cx + (int)(length * cos(angle)),
                  cy + (int)(length * sin(angle)), COLOR_NEEDLE);
  draw_disc(graphics_context, cx, cy, r / 9, COLOR_HUB);
}

static void draw_steering_wheel(const graphics_context_ptr graphics_context,
                                const cockpit_renderer_t* renderer,
                                const dashboard_layout_t* layout,
                                double wheel_angle) {
  if (!renderer->wheel.texture) {
    return;
  }
  int r = layout->wheel_radius;
  SDL_Rect destination = {layout->wheel_x - r, layout->wheel_y - r, 2 * r,
                          2 * r};
  SDL_RenderCopyEx(graphics_context->renderer, renderer->wheel.texture, NULL,
                   &destination, wheel_angle * 180.0 / M_PI, NULL,
                   SDL_FLIP_NONE);
}

void render_cockpit(const graphics_context_ptr graphics_context,
                    const cockpit_renderer_t* renderer, const car_t* car,
                    const car_body_t* body) {
  dashboard_state_t state = dashboard_state(car, body);
  int width = graphics_context->screen_width;
  int height = graphics_context->screen_height;
  dashboard_layout_t layout = dashboard_layout(width, height, &state);
  draw_panel(graphics_context, &layout, width, height);
  draw_dial(graphics_context, &layout, layout.speed_x, layout.dial_y,
            state.speed_needle, false, "MPH");
  draw_dial(graphics_context, &layout, layout.rev_x, layout.dial_y,
            state.rev_needle, true, "RPM");
  draw_steering_wheel(graphics_context, renderer, &layout, state.wheel_angle);
}
