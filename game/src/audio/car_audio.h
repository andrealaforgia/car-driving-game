/**
 * @file car_audio.h
 * @brief Plays the car's sounds through SDL_mixer, following the car's
 * state: the engine is synthesised live in the mixer's music hook, the
 * skid, grass and crash sounds are pre-rendered loops and one-shots. Does
 * nothing if the mixer is not open, so tests and headless tools can skip
 * it.
 */

#ifndef GAME_SRC_AUDIO_CAR_AUDIO_H_
#define GAME_SRC_AUDIO_CAR_AUDIO_H_

#include <SDL.h>
#include <SDL_mixer.h>
#include <stdbool.h>
#include <stdint.h>

#include "car.h"
#include "engine_note.h"
#include "engine_synth.h"

typedef struct {
  Mix_Chunk* chunk;
  int16_t* samples;
} loop_t;

typedef struct {
  bool enabled;
  bool hooked;
  int master_volume;
  int channels;
  engine_synth_t synth; /* owned by the audio thread once hooked */
  SDL_atomic_t rpm_milli;
  SDL_atomic_t throttle;
  SDL_atomic_t volume_milli;
  loop_t skid;
  loop_t grass;
  loop_t thump;
} car_audio_t;

car_audio_t init_car_audio(int master_volume);
/* The struct must stay where it is from the first update until destroyed:
 * the audio thread keeps a pointer to it. */
void destroy_car_audio(car_audio_t* audio);

void set_car_audio_muted(car_audio_t* audio, bool muted, int master_volume);
void update_car_audio(car_audio_t* audio, const car_t* car, car_input_t input);
void play_crash_sound(car_audio_t* audio);

#endif  // GAME_SRC_AUDIO_CAR_AUDIO_H_
