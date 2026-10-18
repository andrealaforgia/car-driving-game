#include "car_audio.h"

#include <stdlib.h>

#include "logger.h"

#define LOOP_MAX_FRAMES 48000
#define SKID_CHANNEL 0
#define GRASS_CHANNEL 1
#define CRASH_CHANNEL 2
#define RESERVED_CHANNELS 3

typedef int (*synthesizer_t)(int16_t*, int, int, int);

static loop_t create_loop(synthesizer_t synthesize, int sample_rate,
                          int channels) {
  loop_t loop = {NULL, NULL};
  loop.samples = calloc((size_t)LOOP_MAX_FRAMES * channels, sizeof(int16_t));
  if (!loop.samples) {
    return loop;
  }
  int frames = synthesize(loop.samples, LOOP_MAX_FRAMES, channels, sample_rate);
  loop.chunk = Mix_QuickLoad_RAW((Uint8*)loop.samples,
                                 (Uint32)(frames * channels * sizeof(int16_t)));
  return loop;
}

static void destroy_loop(loop_t* loop) {
  if (loop->chunk) {
    Mix_FreeChunk(loop->chunk);
    loop->chunk = NULL;
  }
  free(loop->samples);
  loop->samples = NULL;
}

static bool mixer_is_usable(int* sample_rate, int* channels) {
  Uint16 format;
  if (Mix_QuerySpec(sample_rate, &format, channels) == 0) {
    LOG_WARN("Audio mixer is not open; the car will be silent");
    return false;
  }
  if (format != AUDIO_S16SYS) {
    LOG_WARN("Audio mixer is not 16-bit; the car will be silent");
    return false;
  }
  return true;
}

car_audio_t init_car_audio(int master_volume) {
  car_audio_t audio = {0};
  audio.master_volume = master_volume;
  int sample_rate, channels;
  if (!mixer_is_usable(&sample_rate, &channels)) {
    return audio;
  }
  audio.channels = channels;
  audio.synth = create_engine_synth(sample_rate);
  SDL_AtomicSet(&audio.rpm_milli, 0);
  SDL_AtomicSet(&audio.throttle, 0);
  SDL_AtomicSet(&audio.volume_milli, 0);
  audio.skid = create_loop(synthesize_skid_loop, sample_rate, channels);
  audio.grass = create_loop(synthesize_grass_loop, sample_rate, channels);
  audio.thump = create_loop(synthesize_thump, sample_rate, channels);
  Mix_ReserveChannels(RESERVED_CHANNELS);
  audio.enabled = true;
  return audio;
}

/* Runs on the audio thread: fills the music stream with the engine. */
static void render_engine(void* data, Uint8* stream, int length) {
  car_audio_t* audio = data;
  int frames = length / (audio->channels * (int)sizeof(int16_t));
  render_engine_synth(&audio->synth, SDL_AtomicGet(&audio->rpm_milli) / 1000.0,
                      SDL_AtomicGet(&audio->throttle) != 0,
                      SDL_AtomicGet(&audio->volume_milli) / 1000.0,
                      (int16_t*)stream, frames, audio->channels);
}

void destroy_car_audio(car_audio_t* audio) {
  if (audio->hooked) {
    Mix_HookMusic(NULL, NULL);
    audio->hooked = false;
  }
  if (audio->enabled) {
    Mix_HaltChannel(-1);
  }
  destroy_loop(&audio->skid);
  destroy_loop(&audio->grass);
  destroy_loop(&audio->thump);
  audio->enabled = false;
}

void set_car_audio_muted(car_audio_t* audio, bool muted, int master_volume) {
  audio->master_volume = muted ? 0 : master_volume;
}

static int scaled_volume(const car_audio_t* audio, double fraction) {
  return (int)(audio->master_volume * fraction);
}

static void play_loop_on(int channel, const loop_t* loop, int volume) {
  if (!loop->chunk) {
    return;
  }
  if (!Mix_Playing(channel)) {
    Mix_PlayChannel(channel, loop->chunk, -1);
  }
  Mix_Volume(channel, volume);
}

static void stop_loop_on(int channel) {
  if (Mix_Playing(channel)) {
    Mix_HaltChannel(channel);
  }
}

void update_car_audio(car_audio_t* audio, const car_t* car, car_input_t input) {
  if (!audio->enabled) {
    return;
  }
  double rpm = engine_rpm_fraction(car->speed);
  SDL_AtomicSet(&audio->rpm_milli, (int)(rpm * 1000.0));
  bool throttle = input.accelerate || input.reverse;
  SDL_AtomicSet(&audio->throttle, throttle ? 1 : 0);
  SDL_AtomicSet(&audio->volume_milli,
                (int)(engine_volume(throttle, rpm) * audio->master_volume /
                      MIX_MAX_VOLUME * 1000.0));
  if (!audio->hooked) {
    Mix_HookMusic(render_engine, audio);
    audio->hooked = true;
  }

  double skid = skid_volume(input.brake, car->speed);
  if (skid > 0.0) {
    play_loop_on(SKID_CHANNEL, &audio->skid, scaled_volume(audio, skid));
  } else {
    stop_loop_on(SKID_CHANNEL);
  }

  double grass = grass_volume(is_car_off_road(car), car->speed);
  if (grass > 0.0) {
    play_loop_on(GRASS_CHANNEL, &audio->grass, scaled_volume(audio, grass));
  } else {
    stop_loop_on(GRASS_CHANNEL);
  }
}

void play_crash_sound(car_audio_t* audio) {
  if (!audio->enabled || !audio->thump.chunk) {
    return;
  }
  Mix_Volume(CRASH_CHANNEL, audio->master_volume);
  Mix_PlayChannel(CRASH_CHANNEL, audio->thump.chunk, 0);
}
