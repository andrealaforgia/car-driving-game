#include <stdlib.h>
#include <time.h>

#include "audio.h"
#include "car_audio.h"
#include "command_line.h"
#include "display_manager.h"
#include "driving_stage.h"
#include "graphics.h"
#include "keyboard.h"
#include "logger.h"

int main(int argc, char* argv[]) {
  command_line_options_t options = parse_command_line_options(argc, argv);
  if (options.help) {
    print_help();
    return EXIT_SUCCESS;
  }
  if (options.graphics_info) {
    print_graphics_info();
    return EXIT_SUCCESS;
  }

  graphics_context_t graphics_context =
      init_graphics_context(options.display, options.display_mode,
                            options.window_mode, options.vsync);
  keyboard_state_t keyboard = init_keyboard_state();
  audio_context_t audio_context = init_audio_context(1, options.volume);
  car_audio_t car_audio = init_car_audio(options.volume);

  const char* seed_text = getenv("CAR_DRIVING_SEED");
  unsigned track_seed =
      seed_text ? (unsigned)strtoul(seed_text, NULL, 10) : (unsigned)time(NULL);
  LOG_INFO_FMT("Track seed: %u (set CAR_DRIVING_SEED to replay it)",
               track_seed);

  driving_stage_t stage;
  bool succeeded =
      init_driving_stage(&stage, &graphics_context, &keyboard, &car_audio,
                         options.show_fps, options.fps, track_seed);
  if (succeeded) {
    run_driving_stage(&stage);
  } else {
    LOG_ERROR("Unable to build the track");
  }
  destroy_driving_stage(&stage);
  destroy_car_audio(&car_audio);
  terminate_audio_context(&audio_context);
  terminate_graphics_context(&graphics_context);
  return succeeded ? EXIT_SUCCESS : EXIT_FAILURE;
}
