/* Renders one frame headlessly to a BMP so the picture can be checked
 * without opening a window. Usage: render_snapshot out.bmp [position]
 * [car_x] [seed] [lateral_speed] [altitude] [braking] */
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "driving_stage.h"
#include "engine_note.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr,
            "usage: render_snapshot out.bmp [position|lap fraction] [car_x] "
            "[seed]\n");
    return 2;
  }
  double position = argc > 2 ? atof(argv[2]) : 0.0;
  double car_x = argc > 3 ? atof(argv[3]) : 0.0;
  unsigned seed = argc > 4 ? (unsigned)atoi(argv[4]) : 7;
  double lateral_speed = argc > 5 ? atof(argv[5]) : 0.0;
  double altitude = argc > 6 ? atof(argv[6]) : 0.0;
  bool braking = argc > 7 && atoi(argv[7]) != 0;
  int width = 1280;
  int height = 800;

  SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                                        SDL_PIXELFORMAT_RGBA32);
  graphics_context_t graphics_context = {0};
  graphics_context.renderer = SDL_CreateSoftwareRenderer(surface);
  graphics_context.screen_width = width;
  graphics_context.screen_height = height;
  graphics_context.screen_center = point(width / 2, height / 2);

  driving_stage_t stage;
  init_driving_stage(&stage, &graphics_context, NULL, NULL, false, 60, seed);
  /* A position below one is a fraction of the lap. */
  if (position > 0.0 && position < 1.0) {
    position *= stage.track.length;
  }
  stage.car.position = position;
  stage.car.x = car_x;
  stage.car.speed = CAR_MAX_SPEED / 2;
  stage.gear_lever = create_gear_lever(engine_gear(stage.car.speed));
  stage.car.lateral_speed = lateral_speed;
  stage.body.roll = -lateral_speed / CAR_STEERING_RATE;
  stage.body.height = track_height_at(&stage.track, position) + altitude;
  stage.body.altitude = altitude;
  stage.body.airborne = altitude > 0.0;
  stage.last_input.brake = braking;
  render_driving_stage(&stage);
  int saved = SDL_SaveBMP(surface, argv[1]);

  destroy_driving_stage(&stage);
  SDL_DestroyRenderer(graphics_context.renderer);
  SDL_FreeSurface(surface);
  SDL_Quit();
  return saved == 0 ? 0 : 1;
}
