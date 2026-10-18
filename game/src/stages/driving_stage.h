/**
 * @file driving_stage.h
 * @brief The driving loop: input, fixed-step simulation and rendering.
 */

#ifndef GAME_SRC_STAGES_DRIVING_STAGE_H_
#define GAME_SRC_STAGES_DRIVING_STAGE_H_

#include <stdbool.h>

#include "arcade_font.h"
#include "car.h"
#include "car_audio.h"
#include "car_body.h"
#include "cockpit_renderer.h"
#include "fps_tracker.h"
#include "gear_lever.h"
#include "graphics_context.h"
#include "keyboard.h"
#include "lap_timer.h"
#include "projection.h"
#include "road_renderer.h"
#include "road_view.h"
#include "scenery.h"
#include "track.h"
#include "traffic.h"

typedef struct {
  graphics_context_ptr graphics_context;
  car_audio_t* audio; /* may be NULL */
  keyboard_state_ptr keyboard;
  track_t track;
  scenery_t scenery;
  traffic_t traffic;
  car_t car;
  car_body_t body;
  lap_timer_t lap_timer;
  gear_lever_t gear_lever;
  bool crashed; /* a collision happened in the last advance */
  camera_t camera;
  camera_pose_t pose;
  road_view_t view;
  camera_t mirror_camera;
  camera_pose_t mirror_pose;
  road_view_t mirror_view;
  road_renderer_t road_renderer;
  cockpit_renderer_t cockpit_renderer;
  arcade_font_t font; /* for the big speed readout */
  car_input_t last_input;
  double simulation_accumulator;
  bool show_fps;
  bool muted;
  int master_volume;
  int target_fps;
  fps_tracker_t fps_tracker;
} driving_stage_t, *driving_stage_ptr;

bool init_driving_stage(driving_stage_ptr stage,
                        graphics_context_ptr graphics_context,
                        keyboard_state_ptr keyboard, car_audio_t* audio,
                        bool show_fps, int target_fps, unsigned track_seed);
void destroy_driving_stage(driving_stage_ptr stage);

/* delta_time is normalised to the 60 FPS baseline, as the engine reports it */
void advance_driving_stage(driving_stage_ptr stage, car_input_t input,
                           double delta_time);
/* Builds the views for the next frame without drawing them. */
void prepare_driving_stage_frame(driving_stage_ptr stage);
void render_driving_stage(driving_stage_ptr stage);

/* Runs until ESC or the window closes. */
void run_driving_stage(driving_stage_ptr stage);

#endif  // GAME_SRC_STAGES_DRIVING_STAGE_H_
