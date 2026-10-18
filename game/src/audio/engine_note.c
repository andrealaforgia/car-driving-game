#include "engine_note.h"

#include <math.h>

#include "game_constants.h"
#include "geometry.h"

#define GEAR_CHANGE_RPM 0.3
#define SKID_PEAK 11000.0
#define GRASS_PEAK 9000.0
#define LOOP_SECONDS 0.5
#define JOIN_SECONDS 0.02

/* Fraction of the speed limit at which each gear tops out. */
static const double GEAR_TOP[ENGINE_GEARS] = {0.12, 0.22, 0.34, 0.48,
                                              0.64, 0.82, 1.0};

double engine_rpm_fraction(double speed) {
  double fraction = fabs(speed) / CAR_SPEED_LIMIT;
  double low = 0.0;
  for (int gear = 0; gear < ENGINE_GEARS; ++gear) {
    double top = GEAR_TOP[gear];
    if (fraction <= top || gear == ENGINE_GEARS - 1) {
      double in_gear = (fraction - low) / (top - low);
      in_gear = in_gear < 0.0 ? 0.0 : (in_gear > 1.0 ? 1.0 : in_gear);
      double floor_rpm = gear == 0 ? 0.0 : GEAR_CHANGE_RPM;
      return floor_rpm + (1.0 - floor_rpm) * in_gear;
    }
    low = top;
  }
  return 1.0;
}

int engine_gear(double speed) {
  if (speed < 0.0) {
    return GEAR_REVERSE;
  }
  double fraction = speed / CAR_SPEED_LIMIT;
  for (int gear = 0; gear < ENGINE_GEARS - 1; ++gear) {
    if (fraction <= GEAR_TOP[gear]) {
      return gear + 1;
    }
  }
  return ENGINE_GEARS;
}

double engine_volume(bool accelerate, double rpm_fraction) {
  double volume = 0.35 + (accelerate ? 0.35 : 0.0) + 0.3 * rpm_fraction;
  return volume > 1.0 ? 1.0 : volume;
}

double skid_volume(bool braking, double speed) {
  double fraction = fabs(speed) / CAR_MAX_SPEED;
  if (!braking || fraction < 0.15) {
    return 0.0;
  }
  return fraction > 1.0 ? 1.0 : fraction;
}

double grass_volume(bool off_road, double speed) {
  double fraction = fabs(speed) / CAR_MAX_SPEED;
  if (!off_road || fraction < 0.05) {
    return 0.0;
  }
  return 0.3 + (fraction > 1.0 ? 0.7 : 0.7 * fraction);
}

static void write_frame(int16_t* samples, int frame, int channels,
                        double value) {
  value = value > 32767.0 ? 32767.0 : (value < -32768.0 ? -32768.0 : value);
  int16_t sample = (int16_t)value;
  for (int channel = 0; channel < channels; ++channel) {
    samples[frame * channels + channel] = sample;
  }
}

static unsigned noise_state = 1u;

static double next_noise(void) {
  noise_state = noise_state * 1664525u + 1013904223u;
  return (noise_state >> 8) / 8388607.5 - 1.0;
}

/* Coloured noise through a one-pole low-pass, with the last few
 * milliseconds blended into the first so the loop joins softly. */
static int synthesize_noise_loop(int16_t* samples, int max_frames, int channels,
                                 int sample_rate, double smoothing, double peak,
                                 unsigned seed) {
  int frames = (int)(LOOP_SECONDS * sample_rate);
  if (frames > max_frames) {
    frames = max_frames;
  }
  int join = (int)(JOIN_SECONDS * sample_rate);
  noise_state = seed;
  double filtered = 0.0;
  double previous_head[2048];
  for (int frame = 0; frame < frames; ++frame) {
    filtered += (next_noise() - filtered) * smoothing;
    double value = filtered * peak;
    if (frame < join && frame < 2048) {
      previous_head[frame] = value;
    }
    if (frame >= frames - join) {
      int into_join = frame - (frames - join);
      double blend = (double)into_join / join;
      int head_index = into_join < 2048 ? into_join : 2047;
      value = value * (1.0 - blend) + previous_head[head_index] * blend;
    }
    write_frame(samples, frame, channels, value);
  }
  return frames;
}

int synthesize_skid_loop(int16_t* samples, int max_frames, int channels,
                         int sample_rate) {
  return synthesize_noise_loop(samples, max_frames, channels, sample_rate, 0.35,
                               SKID_PEAK * 2.2, 7u);
}

int synthesize_grass_loop(int16_t* samples, int max_frames, int channels,
                          int sample_rate) {
  return synthesize_noise_loop(samples, max_frames, channels, sample_rate, 0.08,
                               GRASS_PEAK * 4.0, 11u);
}

#define THUMP_SECONDS 0.3
#define THUMP_DECAY_SECONDS 0.06
#define THUMP_HZ 55.0
#define THUMP_PEAK 15000.0

int synthesize_thump(int16_t* samples, int max_frames, int channels,
                     int sample_rate) {
  int frames = (int)(THUMP_SECONDS * sample_rate);
  if (frames > max_frames) {
    frames = max_frames;
  }
  unsigned noise = 99u;
  double filtered = 0.0;
  for (int i = 0; i < frames; ++i) {
    double t = (double)i / sample_rate;
    noise = noise * 1664525u + 1013904223u;
    double white = ((noise >> 8) % 20001) / 10000.0 - 1.0;
    filtered += (white - filtered) * 0.08;
    double envelope =
        exp(-t / THUMP_DECAY_SECONDS) * (1.0 - (double)i / frames);
    double value = (0.6 * sin(2.0 * M_PI * THUMP_HZ * t) + 0.8 * filtered) *
                   envelope * THUMP_PEAK;
    int16_t sample =
        (int16_t)(i == frames - 1 ? 0.0 : fmax(-32000.0, fmin(32000.0, value)));
    for (int channel = 0; channel < channels; ++channel) {
      samples[i * channels + channel] = sample;
    }
  }
  return frames;
}
