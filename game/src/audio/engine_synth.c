#include "engine_synth.h"

#include <math.h>

#include "geometry.h"

#define IDLE_FIRING_HZ 26.0
#define RED_LINE_FIRING_HZ 300.0
#define REV_TRACKING_RATE 12.0
#define THROTTLE_TRACKING_RATE 6.0
#define CYCLE_JITTER 0.06
#define GAIN_JITTER 0.35
#define PULSE_LENGTH_CYCLES 0.3
#define THUMP_HZ 95.0
#define THUMP_BANDWIDTH_HZ 60.0
#define BARK_HZ 360.0
#define BARK_BANDWIDTH_HZ 110.0
#define PULSE_GAIN 3.0
#define DRIVE 2.0
#define PEAK 20000.0

static resonator_t create_resonator(double centre_hz, double bandwidth_hz,
                                    int sample_rate);

engine_synth_t create_engine_synth(int sample_rate) {
  engine_synth_t synth;
  synth.sample_rate = sample_rate;
  synth.rpm = 0.0;
  synth.throttle = 0.0;
  synth.phase = 0.0;
  synth.cycle_stretch = 1.0;
  synth.pulse = 0.0;
  synth.pulse_gain = 1.0;
  synth.bed = 0.0;
  synth.thump = create_resonator(THUMP_HZ, THUMP_BANDWIDTH_HZ, sample_rate);
  synth.bark = create_resonator(BARK_HZ, BARK_BANDWIDTH_HZ, sample_rate);
  synth.noise = 2024u;
  synth.firings = 0;
  synth.cycle_seconds = 0.0;
  synth.shortest_cycle = 1e9;
  synth.longest_cycle = 0.0;
  return synth;
}

double engine_firing_hz(double rpm_fraction) {
  rpm_fraction =
      rpm_fraction < 0.0 ? 0.0 : (rpm_fraction > 1.0 ? 1.0 : rpm_fraction);
  return IDLE_FIRING_HZ *
         pow(RED_LINE_FIRING_HZ / IDLE_FIRING_HZ, rpm_fraction);
}

static double next_noise(engine_synth_t* synth) {
  synth->noise = synth->noise * 1664525u + 1013904223u;
  return (synth->noise >> 8) / 8388607.5 - 1.0;
}

/* A two-pole band-pass resonator with zeros at 0 Hz and the Nyquist
 * frequency: the ring of an exhaust pipe, passing no DC. Unit gain at
 * the centre frequency. */
static resonator_t create_resonator(double centre_hz, double bandwidth_hz,
                                    int sample_rate) {
  resonator_t state;
  double w = 2.0 * M_PI * centre_hz / sample_rate;
  state.r = exp(-M_PI * bandwidth_hz / sample_rate);
  state.cos_w = cos(w);
  double numerator = hypot(1.0 - cos(2.0 * w), sin(2.0 * w));
  double denominator_real = 1.0 - 2.0 * state.r * state.cos_w * cos(w) +
                            state.r * state.r * cos(2.0 * w);
  double denominator_imaginary =
      2.0 * state.r * state.cos_w * sin(w) - state.r * state.r * sin(2.0 * w);
  state.gain = hypot(denominator_real, denominator_imaginary) / numerator;
  state.x1 = state.x2 = state.y1 = state.y2 = 0.0;
  return state;
}

static double resonate(resonator_t* state, double input) {
  double y = state->gain * (input - state->x2) +
             2.0 * state->r * state->cos_w * state->y1 -
             state->r * state->r * state->y2;
  state->x2 = state->x1;
  state->x1 = input;
  state->y2 = state->y1;
  state->y1 = y;
  return y;
}

static void start_firing(engine_synth_t* synth) {
  if (synth->firings > 0) {
    synth->shortest_cycle = fmin(synth->shortest_cycle, synth->cycle_seconds);
    synth->longest_cycle = fmax(synth->longest_cycle, synth->cycle_seconds);
  }
  ++synth->firings;
  synth->cycle_seconds = 0.0;
  synth->cycle_stretch = 1.0 + CYCLE_JITTER * next_noise(synth);
  synth->pulse_gain = 1.0 + GAIN_JITTER * next_noise(synth);
  synth->pulse = 1.0;
}

static double next_sample(engine_synth_t* synth, double rpm_target,
                          double throttle_target) {
  double dt = 1.0 / synth->sample_rate;
  synth->rpm += (rpm_target - synth->rpm) * REV_TRACKING_RATE * dt;
  synth->throttle +=
      (throttle_target - synth->throttle) * THROTTLE_TRACKING_RATE * dt;
  double hz = engine_firing_hz(synth->rpm);
  synth->phase += hz * dt / synth->cycle_stretch;
  synth->cycle_seconds += dt;
  if (synth->phase >= 1.0) {
    synth->phase -= 1.0;
    start_firing(synth);
  }
  synth->pulse *= exp(-hz * dt / PULSE_LENGTH_CYCLES);
  double burst =
      synth->pulse * synth->pulse_gain * (0.4 + 0.6 * next_noise(synth));
  double pressure = 0.4 + 0.6 * synth->throttle;
  double pulses = 1.0 * resonate(&synth->thump, burst) +
                  0.7 * resonate(&synth->bark, burst);
  double smoothing = 0.03 + 0.2 * synth->rpm;
  synth->bed += (next_noise(synth) - synth->bed) * smoothing;
  double hiss =
      synth->bed * (0.06 + 0.2 * synth->throttle) * (0.3 + 0.7 * synth->rpm);
  return tanh(DRIVE * (pulses * PULSE_GAIN * pressure + hiss));
}

void render_engine_synth(engine_synth_t* synth, double rpm_fraction,
                         bool throttle, double volume, int16_t* samples,
                         int frames, int channels) {
  double throttle_target = throttle ? 1.0 : 0.0;
  for (int frame = 0; frame < frames; ++frame) {
    double value =
        next_sample(synth, rpm_fraction, throttle_target) * volume * PEAK;
    int16_t sample =
        (int16_t)(value > 32767.0 ? 32767.0
                                  : (value < -32768.0 ? -32768.0 : value));
    for (int channel = 0; channel < channels; ++channel) {
      samples[frame * channels + channel] = sample;
    }
  }
}
