#include "cockpit_renderer.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "dashboard.h"
#include "dial_sprite.h"
#include "drawing_primitives.h"
#include "engine_note.h"
#include "game_constants.h"
#include "gear_gate_sprite.h"
#include "geometry.h"
#include "steering_wheel_sprite.h"
#include "text.h"

#define COLOR_PANEL 0x1A1A1E
#define COLOR_PANEL_EDGE 0x55555E
#define COLOR_TICK 0xE8E8E8
#define COLOR_RED_ZONE 0xD02020
#define COLOR_LABEL 0xC0C0C8
#define COLOR_ENGRAVE_DARK 0x2E2E34
#define COLOR_ENGRAVE_LIGHT 0xD8D8DE

/* The lever's height relative to the plate, and how far (degrees) it
 * leans sideways from one edge of the gate to the other. */
#define LEVER_HEIGHT_FACTOR 1.2
#define LEVER_LEAN 26.0

/* The road shows through the band. */
#define PANEL_ALPHA 96

#define RED_ZONE_FROM 0.85
#define SPEED_DIAL_STEP 25
#define SPEED_DIAL_MAJORS ((int)(CAR_TOP_SPEED_MPH / SPEED_DIAL_STEP))
#define REV_DIAL_MAJORS 8

static texture_t upload_sprite(SDL_Renderer* renderer, int width, int height,
                               void (*paint)(uint32_t*, int, int)) {
  texture_t texture = {NULL, width, height};
  if (!renderer) {
    return texture;
  }
  uint32_t* pixels = calloc((size_t)width * height, sizeof(uint32_t));
  if (!pixels) {
    return texture;
  }
  paint(pixels, width, height);
  texture.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STATIC, width, height);
  if (texture.texture) {
    SDL_UpdateTexture(texture.texture, NULL, pixels,
                      width * (int)sizeof(uint32_t));
    SDL_SetTextureBlendMode(texture.texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture.texture, SDL_ScaleModeLinear);
  }
  free(pixels);
  return texture;
}

static void paint_wheel(uint32_t* pixels, int width, int height) {
  (void)height;
  render_steering_wheel_pixels(pixels, width);
}

static void paint_speed_face(uint32_t* pixels, int width, int height) {
  (void)height;
  render_dial_face_pixels(pixels, width, SPEED_DIAL_MAJORS, 2.0);
}

static void paint_rev_face(uint32_t* pixels, int width, int height) {
  (void)height;
  render_dial_face_pixels(pixels, width, REV_DIAL_MAJORS, RED_ZONE_FROM);
}

cockpit_renderer_t init_cockpit_renderer(
    const graphics_context_ptr graphics_context) {
  cockpit_renderer_t renderer;
  renderer.wheel = upload_sprite(graphics_context->renderer, WHEEL_SPRITE_SIZE,
                                 WHEEL_SPRITE_SIZE, paint_wheel);
  renderer.gate = upload_sprite(graphics_context->renderer, GATE_SPRITE_WIDTH,
                                GATE_SPRITE_HEIGHT, render_gear_gate_pixels);
  renderer.lever = upload_sprite(graphics_context->renderer, LEVER_SPRITE_WIDTH,
                                 LEVER_SPRITE_HEIGHT, render_gear_lever_pixels);
  renderer.speed_face =
      upload_sprite(graphics_context->renderer, DIAL_SPRITE_SIZE,
                    DIAL_SPRITE_SIZE, paint_speed_face);
  renderer.rev_face =
      upload_sprite(graphics_context->renderer, DIAL_SPRITE_SIZE,
                    DIAL_SPRITE_SIZE, paint_rev_face);
  renderer.needle =
      upload_sprite(graphics_context->renderer, NEEDLE_SPRITE_WIDTH,
                    NEEDLE_SPRITE_HEIGHT, render_needle_pixels);
  return renderer;
}

void destroy_cockpit_renderer(cockpit_renderer_t* renderer) {
  free_texture(&renderer->wheel);
  free_texture(&renderer->gate);
  free_texture(&renderer->lever);
  free_texture(&renderer->speed_face);
  free_texture(&renderer->rev_face);
  free_texture(&renderer->needle);
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

/* A dial: the painted face, numbers lettered inside the ticks, a label,
 * and the needle sprite turned about its pivot over its own shadow. */
static void draw_dial(const graphics_context_ptr graphics_context,
                      const cockpit_renderer_t* renderer,
                      const dashboard_layout_t* layout, const texture_t* face,
                      int cx, int cy, double needle, int majors, int label_step,
                      const char* label) {
  int r = layout->dial_radius;
  if (!face->texture || !renderer->needle.texture) {
    return;
  }
  SDL_Rect plate = {cx - r, cy - r, 2 * r, 2 * r};
  SDL_RenderCopy(graphics_context->renderer, face->texture, NULL, &plate);
  int scale = r / 40 > 0 ? r / 40 : 1;
  for (int major = 0; major <= majors; ++major) {
    double angle = dial_angle((double)major / majors);
    char number[16];
    snprintf(number, sizeof number, "%d", major * label_step);
    text_dimensions_t size = calculate_text_dimensions(number, scale);
    double reach = r * (DIAL_TICK_RADIUS - 0.14) - size.height * 0.7;
    write_text(graphics_context, number,
               point(cx + (int)(reach * cos(angle)) - size.width / 2,
                     cy + (int)(reach * sin(angle)) + size.height / 2),
               scale, COLOR_TICK);
  }
  text_dimensions_t size = calculate_text_dimensions(label, scale);
  write_text(graphics_context, label, point(cx - size.width / 2, cy + r / 2),
             scale, COLOR_LABEL);
  double angle = dial_angle(0.0) + needle;
  double degrees = (angle + M_PI / 2.0) * 180.0 / M_PI;
  int height = (int)(r * NEEDLE_REACH / (NEEDLE_PIVOT_Y - 0.03));
  int width = height * NEEDLE_SPRITE_WIDTH / NEEDLE_SPRITE_HEIGHT;
  int pivot = (int)(height * NEEDLE_PIVOT_Y);
  SDL_Point centre = {width / 2, pivot};
  int drop = r / 30 + 1;
  SDL_Rect shadow = {cx - width / 2 + drop, cy - pivot + drop, width, height};
  SDL_SetTextureColorMod(renderer->needle.texture, 0, 0, 0);
  SDL_SetTextureAlphaMod(renderer->needle.texture, 110);
  SDL_RenderCopyEx(graphics_context->renderer, renderer->needle.texture, NULL,
                   &shadow, degrees, &centre, SDL_FLIP_NONE);
  SDL_SetTextureColorMod(renderer->needle.texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(renderer->needle.texture, 255);
  SDL_Rect destination = {cx - width / 2, cy - pivot, width, height};
  SDL_RenderCopyEx(graphics_context->renderer, renderer->needle.texture, NULL,
                   &destination, degrees, &centre, SDL_FLIP_NONE);
}

/* Numbers engraved into the plate beside the slot ends: a light copy
 * offset down and right under a dark one. */
static void engrave_gear_numbers(const graphics_context_ptr graphics_context,
                                 const SDL_Rect* plate, int gear) {
  int scale = plate->h / 70 > 0 ? plate->h / 70 : 1;
  for (int number = 1; number <= ENGINE_GEARS; ++number) {
    gear_slot_t slot = gear_gate_slot(number);
    char text[16];
    snprintf(text, sizeof text, "%d", number);
    text_dimensions_t size = calculate_text_dimensions(text, scale);
    int x =
        plate->x + (int)(gate_slot_x(slot.column) * plate->w) - size.width / 2;
    int slot_end = plate->y + (int)(gate_slot_y(slot.row) * plate->h) +
                   slot.row * (int)(0.07 * plate->h);
    int baseline = slot.row < 0 ? slot_end - 2 : slot_end + size.height + 2;
    write_text(graphics_context, text, point(x + 1, baseline + 1), scale,
               COLOR_ENGRAVE_LIGHT);
    write_text(graphics_context, text, point(x, baseline), scale,
               number == gear ? COLOR_RED_ZONE : COLOR_ENGRAVE_DARK);
  }
}

/* The gate plate with the lever standing in it, leaning with its slot:
 * sideways away from the middle, and towards the driver (so a little
 * shorter) in the bottom row. */
static void draw_gear_gate(const graphics_context_ptr graphics_context,
                           const cockpit_renderer_t* renderer,
                           const dashboard_layout_t* layout,
                           const gear_lever_t* lever, int gear) {
  if (!renderer->gate.texture || !renderer->lever.texture) {
    return;
  }
  SDL_Rect plate = {layout->gate_x - layout->gate_width / 2,
                    layout->gate_y - layout->gate_height / 2,
                    layout->gate_width, layout->gate_height};
  SDL_RenderCopy(graphics_context->renderer, renderer->gate.texture, NULL,
                 &plate);
  engrave_gear_numbers(graphics_context, &plate, gear);
  double base_x = plate.x + gate_slot_x(lever->column) * plate.w;
  double base_y = plate.y + gate_slot_y(lever->row) * plate.h;
  double towards_driver = (lever->row + 1.0) / 2.0;
  int height =
      (int)(plate.h * LEVER_HEIGHT_FACTOR * (1.0 - 0.15 * towards_driver));
  int width = height * LEVER_SPRITE_WIDTH / LEVER_SPRITE_HEIGHT;
  int pivot = (int)(height * LEVER_BASE_Y);
  SDL_Rect destination = {(int)base_x - width / 2, (int)base_y - pivot, width,
                          height};
  SDL_Point centre = {width / 2, pivot};
  double lean = (base_x - layout->gate_x) / layout->gate_width * LEVER_LEAN;
  SDL_RenderCopyEx(graphics_context->renderer, renderer->lever.texture, NULL,
                   &destination, lean, &centre, SDL_FLIP_NONE);
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
                    const car_body_t* body, const gear_lever_t* lever) {
  dashboard_state_t state = dashboard_state(car, body);
  int width = graphics_context->screen_width;
  int height = graphics_context->screen_height;
  dashboard_layout_t layout = dashboard_layout(width, height, &state);
  draw_panel(graphics_context, &layout, width, height);
  draw_dial(graphics_context, renderer, &layout, &renderer->speed_face,
            layout.speed_x, layout.dial_y, state.speed_needle,
            SPEED_DIAL_MAJORS, SPEED_DIAL_STEP, "MPH");
  draw_dial(graphics_context, renderer, &layout, &renderer->rev_face,
            layout.rev_x, layout.dial_y, state.rev_needle, REV_DIAL_MAJORS, 1,
            "RPM X1000");
  draw_gear_gate(graphics_context, renderer, &layout, lever, state.gear);
  draw_steering_wheel(graphics_context, renderer, &layout, state.wheel_angle);
}
