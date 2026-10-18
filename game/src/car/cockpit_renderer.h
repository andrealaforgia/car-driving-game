/**
 * @file cockpit_renderer.h
 * @brief Draws the driver's view of the car: a translucent dashboard low
 * across the screen with a speedometer, a rev counter and a steering
 * wheel sprite that turns with the steering and an open gear gate whose
 * lever slides between the slots, all moving with the body.
 */

#ifndef GAME_SRC_CAR_COCKPIT_RENDERER_H_
#define GAME_SRC_CAR_COCKPIT_RENDERER_H_

#include "car.h"
#include "car_body.h"
#include "gear_lever.h"
#include "graphics_context.h"
#include "texture.h"

typedef struct {
  texture_t wheel;
  texture_t gate;
  texture_t lever;
  texture_t speed_face;
  texture_t rev_face;
  texture_t needle;
} cockpit_renderer_t;

cockpit_renderer_t init_cockpit_renderer(
    const graphics_context_ptr graphics_context);
void destroy_cockpit_renderer(cockpit_renderer_t* renderer);

void render_cockpit(const graphics_context_ptr graphics_context,
                    const cockpit_renderer_t* renderer, const car_t* car,
                    const car_body_t* body, const gear_lever_t* lever);

#endif  // GAME_SRC_CAR_COCKPIT_RENDERER_H_
