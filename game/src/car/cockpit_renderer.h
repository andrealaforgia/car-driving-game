/**
 * @file cockpit_renderer.h
 * @brief Draws the driver's view of the car: a translucent dashboard low
 * across the screen with a speedometer, a rev counter and a steering
 * wheel sprite that turns with the steering, all moving with the body.
 */

#ifndef GAME_SRC_CAR_COCKPIT_RENDERER_H_
#define GAME_SRC_CAR_COCKPIT_RENDERER_H_

#include "car.h"
#include "car_body.h"
#include "graphics_context.h"
#include "texture.h"

typedef struct {
  texture_t wheel;
} cockpit_renderer_t;

cockpit_renderer_t init_cockpit_renderer(
    const graphics_context_ptr graphics_context);
void destroy_cockpit_renderer(cockpit_renderer_t* renderer);

void render_cockpit(const graphics_context_ptr graphics_context,
                    const cockpit_renderer_t* renderer, const car_t* car,
                    const car_body_t* body);

#endif  // GAME_SRC_CAR_COCKPIT_RENDERER_H_
