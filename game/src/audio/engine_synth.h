/**
 * @file engine_synth.h
 * @brief A live engine sound: a train of exhaust pulses at a firing rate
 * that follows the revs continuously.
 *
 * Each firing is a short burst of noise with a sharp onset, pushed through
 * two resonant pipe filters; the time between firings and their strength
 * vary a little from one to the next so it sounds like a motor rather than
 * a note. Under it sits a bed of intake noise that grows with the revs and
 * the throttle. Pure: no SDL. The renderer is called from the audio
 * thread with the targets it should follow.
 */

#ifndef GAME_SRC_AUDIO_ENGINE_SYNTH_H_
#define GAME_SRC_AUDIO_ENGINE_SYNTH_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  double r;
  double cos_w;
  double gain;
  double x1;
  double x2;
  double y1;
  double y2;
} resonator_t;

typedef struct {
  int sample_rate;
  double rpm;      /* smoothed rev fraction, 0 idle to 1 red line */
  double throttle; /* smoothed, 0 to 1 */
  double phase;    /* position within the current firing cycle, 0 to 1 */
  double cycle_stretch;
  double pulse;
  double pulse_gain;
  double bed;
  resonator_t thump;
  resonator_t bark;
  unsigned noise;
  int firings;
  double cycle_seconds;
  double shortest_cycle;
  double longest_cycle;
} engine_synth_t;

engine_synth_t create_engine_synth(int sample_rate);

/* Firings per second at a rev fraction; continuous and rising. */
double engine_firing_hz(double rpm_fraction);

/* Writes frames of interleaved 16-bit samples, the same in every channel,
 * easing the engine towards the given revs and throttle. */
void render_engine_synth(engine_synth_t* synth, double rpm_fraction,
                         bool throttle, double volume, int16_t* samples,
                         int frames, int channels);

#endif  // GAME_SRC_AUDIO_ENGINE_SYNTH_H_
