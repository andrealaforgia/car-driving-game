/**
 * @file engine_note.h
 * @brief The car's sounds apart from the engine itself: a gearbox model
 * that turns speed into revs, the volume rules, and the waveforms for a
 * braking skid, tyres on grass and a crash. The engine is synthesised
 * live in engine_synth.h.
 *
 * Pure: no SDL. Samples are interleaved signed 16-bit frames.
 */

#ifndef GAME_SRC_AUDIO_ENGINE_NOTE_H_
#define GAME_SRC_AUDIO_ENGINE_NOTE_H_

#include <stdbool.h>
#include <stdint.h>

#define ENGINE_GEARS 7

/* 0 at rest, 1 at the red line; drops at each gear change. */
double engine_rpm_fraction(double speed);
int engine_gear(double speed);

double engine_volume(bool accelerate, double rpm_fraction);
double skid_volume(bool braking, double speed);
double grass_volume(bool off_road, double speed);

/* Each returns the number of frames written, a seamless loop. */
int synthesize_skid_loop(int16_t* samples, int max_frames, int channels,
                         int sample_rate);
/* A one-shot crash thump; returns the frames written. */
int synthesize_thump(int16_t* samples, int max_frames, int channels,
                     int sample_rate);
int synthesize_grass_loop(int16_t* samples, int max_frames, int channels,
                          int sample_rate);

#endif  // GAME_SRC_AUDIO_ENGINE_NOTE_H_
