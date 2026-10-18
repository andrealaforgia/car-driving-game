#include "driving_stage.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "cockpit_renderer.h"
#include "color.h"
#include "dashboard.h"
#include "display_manager.h"
#include "drawing_primitives.h"
#include "engine_note.h"
#include "events.h"
#include "frame.h"
#include "frame_limiter.h"
#include "game_constants.h"
#include "road_renderer.h"
#include "text.h"
#include "track_generator.h"

bool init_driving_stage(driving_stage_ptr stage,
                        graphics_context_ptr graphics_context,
                        keyboard_state_ptr keyboard, car_audio_t* audio,
                        bool show_fps, int target_fps, unsigned track_seed) {
  stage->graphics_context = graphics_context;
  stage->audio = audio;
  stage->keyboard = keyboard;
  stage->track = generate_track(track_seed);
  stage->scenery = place_scenery(&stage->track, track_seed);
  stage->traffic =
      create_traffic(track_seed, stage->track.length, TRAFFIC_COUNT);
  stage->car = create_car();
  stage->body = create_car_body(0.0);
  stage->lap_timer = create_lap_timer();
  stage->gear_lever = create_gear_lever(engine_gear(0.0));
  stage->crashed = false;
  stage->camera = create_camera(
      CAMERA_FIELD_OF_VIEW_DEGREES,
      (double)graphics_context->screen_width / graphics_context->screen_height);
  stage->pose = cockpit_camera_pose(&stage->track, 0.0, 0.0, 0.0, 0.0);
  stage->view.polygon_count = 0;
  stage->view.order_count = 0;
  dashboard_state_t still = {0.0, 0.0, 0.0, 0.0, 0.0, 1};
  dashboard_layout_t layout = dashboard_layout(
      graphics_context->screen_width, graphics_context->screen_height, &still);
  stage->mirror_camera =
      create_camera(MIRROR_FIELD_OF_VIEW_DEGREES,
                    (double)layout.mirror_width / layout.mirror_height);
  stage->mirror_pose = mirror_camera_pose(&stage->track, 0.0, 0.0, 0.0);
  stage->mirror_view.polygon_count = 0;
  stage->mirror_view.order_count = 0;
  stage->road_renderer = init_road_renderer(graphics_context);
  stage->cockpit_renderer = init_cockpit_renderer(graphics_context);
  stage->font = (arcade_font_t){0};
  if (graphics_context->renderer) {
    stage->font = load_arcade_font(graphics_context);
  }
  stage->last_input = (car_input_t){false, false, false, false};
  stage->simulation_accumulator = 0.0;
  stage->show_fps = show_fps;
  stage->muted = false;
  stage->master_volume = audio ? audio->master_volume : 0;
  stage->target_fps = target_fps;
  stage->fps_tracker = create_fps_tracker();
  return stage->track.count > 0;
}

void destroy_driving_stage(driving_stage_ptr stage) {
  destroy_scenery(&stage->scenery);
  destroy_track(&stage->track);
  destroy_road_renderer(&stage->road_renderer);
  destroy_cockpit_renderer(&stage->cockpit_renderer);
  free_arcade_font(&stage->font);
}

static void crash(driving_stage_ptr stage) {
  stage->car.speed *= CRASH_SPEED_FACTOR;
  stage->car.lateral_speed = -stage->car.lateral_speed;
  jolt_car_body(&stage->body, CRASH_JOLT);
  stage->crashed = true;
}

static void collide(driving_stage_ptr stage) {
  if (hit_traffic(&stage->traffic, &stage->car, stage->track.length)) {
    jolt_car_body(&stage->body, CRASH_JOLT / 2.0);
    stage->crashed = true;
  }
  if (!stage->body.airborne && scenery_blocks(&stage->scenery, &stage->car)) {
    crash(stage);
  }
}

static void simulate_step(driving_stage_ptr stage, car_input_t input,
                          double seconds) {
  const track_t* track = &stage->track;
  const segment_t* segment =
      &track->segments[segment_index_at(track, stage->car.position)];
  road_conditions_t road = {segment->curve, segment_slope(segment),
                            stage->body.airborne};
  double previous_position = stage->car.position;
  update_car(&stage->car, input, seconds, track->length, road);
  update_traffic(&stage->traffic, seconds, track->length, &stage->car);
  collide(stage);
  update_lap_timer(&stage->lap_timer, seconds, previous_position,
                   stage->car.position, track->length);
  update_gear_lever(&stage->gear_lever, engine_gear(stage->car.speed), seconds);
  segment = &track->segments[segment_index_at(track, stage->car.position)];
  update_car_body(&stage->body, &stage->car, seconds,
                  track_height_at(track, stage->car.position),
                  segment_slope(segment));
}

void advance_driving_stage(driving_stage_ptr stage, car_input_t input,
                           double delta_time) {
  if (!isfinite(delta_time) || delta_time <= 0) {
    return;
  }
  stage->last_input = input;
  stage->crashed = false;
  const double step = PHYSICS_BASELINE_FPS / SIMULATION_STEPS_PER_SECOND;
  const double max_delta =
      MAX_FRAME_CATCH_UP_MS * PHYSICS_BASELINE_FPS / 1000.0;
  stage->simulation_accumulator += fmin(delta_time, max_delta);
  while (stage->simulation_accumulator + 1e-9 >= step) {
    stage->simulation_accumulator =
        fmax(0, stage->simulation_accumulator - step);
    simulate_step(stage, input, 1.0 / SIMULATION_STEPS_PER_SECOND);
  }
}

static void write_right_aligned(driving_stage_ptr stage, const char* text,
                                int top, int scale) {
  text_dimensions_t dimensions = calculate_text_dimensions(text, scale);
  write_text(
      stage->graphics_context, text,
      point(stage->graphics_context->screen_width - dimensions.width - 10,
            top + dimensions.height),
      scale, COLOR_WHITE);
}

/* The HUD font has no punctuation, so a lap time is written in three
 * pieces with a colon and a dot drawn as little squares between them. */
typedef struct {
  char minutes[8];
  char seconds[4];
  char tenths[2];
} lap_time_pieces_t;

static lap_time_pieces_t split_lap_time(double seconds) {
  char text[16];
  lap_time_pieces_t pieces;
  format_lap_time(seconds, text, sizeof text);
  sscanf(text, "%7[^:]:%3[^.].%1s", pieces.minutes, pieces.seconds,
         pieces.tenths);
  return pieces;
}

static void write_lap_time(driving_stage_ptr stage, const char* label,
                           double seconds, int top, int scale) {
  lap_time_pieces_t pieces = split_lap_time(seconds);
  char head[32];
  snprintf(head, sizeof head, "%s %s", label, pieces.minutes);
  int mark = scale;
  int gap = 4 * scale;
  text_dimensions_t head_size = calculate_text_dimensions(head, scale);
  text_dimensions_t seconds_size =
      calculate_text_dimensions(pieces.seconds, scale);
  text_dimensions_t tenths_size =
      calculate_text_dimensions(pieces.tenths, scale);
  int width =
      head_size.width + gap + seconds_size.width + gap + tenths_size.width;
  int height = head_size.height;
  int x = stage->graphics_context->screen_width - width - 10;
  int baseline = top + height;
  point_t end = write_text(stage->graphics_context, head, point(x, baseline),
                           scale, COLOR_WHITE);
  int colon_x = (int)end.x + (gap - mark) / 2;
  draw_filled_rect(stage->graphics_context, colon_x, baseline - height, mark,
                   mark, COLOR_WHITE);
  draw_filled_rect(stage->graphics_context, colon_x, baseline - mark, mark,
                   mark, COLOR_WHITE);
  end = write_text(stage->graphics_context, pieces.seconds,
                   point(end.x + gap, baseline), scale, COLOR_WHITE);
  draw_filled_rect(stage->graphics_context, (int)end.x + (gap - mark) / 2,
                   baseline - mark, mark, mark, COLOR_WHITE);
  write_text(stage->graphics_context, pieces.tenths,
             point(end.x + gap, baseline), scale, COLOR_WHITE);
}

static void render_hud(driving_stage_ptr stage) {
  int scale = stage->graphics_context->screen_height * 5 / 900;
  if (scale < 1) {
    scale = 1;
  }
  char text[48];
  format_speed(stage->car.speed, text, sizeof text);
  int speed_scale = scale * 2;
  dashboard_state_t still = {0.0, 0.0, 0.0, 0.0, 0.0, 1};
  dashboard_layout_t layout =
      dashboard_layout(stage->graphics_context->screen_width,
                       stage->graphics_context->screen_height, &still);
  int under_mirror = layout.mirror_y + layout.mirror_height / 2 + 4 * scale;
  int speed_width =
      get_arcade_text_width_scaled(&stage->font, text, speed_scale);
  render_arcade_text_scaled(
      &stage->font, stage->graphics_context, text,
      (stage->graphics_context->screen_width - speed_width) / 2, under_mirror,
      FONT_COLOR_WHITE, speed_scale);
  snprintf(text, sizeof text, "GEAR %d", engine_gear(stage->car.speed));
  write_right_aligned(stage, text, 10, scale);
  snprintf(text, sizeof text, "LAP %d ", stage->lap_timer.laps + 1);
  write_lap_time(stage, text, stage->lap_timer.current, 10 + 12 * scale, scale);
  if (stage->lap_timer.has_best) {
    write_lap_time(stage, "BEST ", stage->lap_timer.best, 10 + 24 * scale,
                   scale);
  }
  track_fps(&stage->fps_tracker);
  if (stage->show_fps) {
    char fps_text[10] = {0};
    format_fps(&stage->fps_tracker, fps_text, sizeof fps_text);
    text_dimensions_t fps_dimensions =
        calculate_text_dimensions(fps_text, scale);
    write_text(stage->graphics_context, fps_text,
               point(10, 10 + fps_dimensions.height), scale, COLOR_WHITE);
  }
}

static void add_scenery_and_traffic(driving_stage_ptr stage,
                                    road_view_t* view) {
  for (int i = 0; i < stage->scenery.count; ++i) {
    add_billboard_to_view(view, &stage->scenery.items[i].billboard);
  }
  for (int i = 0; i < stage->traffic.count; ++i) {
    billboard_t car = traffic_billboard(&stage->track, &stage->traffic.cars[i]);
    add_billboard_to_view(view, &car);
  }
}

void prepare_driving_stage_frame(driving_stage_ptr stage) {
  stage->pose =
      cockpit_camera_pose(&stage->track, stage->car.position, stage->car.x,
                          stage->body.height, stage->body.pitch);
  begin_road_view(&stage->view, &stage->camera, &stage->pose);
  add_track_to_view(&stage->view, &stage->track, stage->car.position);
  add_scenery_and_traffic(stage, &stage->view);
  finish_road_view(&stage->view);

  stage->mirror_pose = mirror_camera_pose(&stage->track, stage->car.position,
                                          stage->car.x, stage->body.height);
  begin_road_view(&stage->mirror_view, &stage->mirror_camera,
                  &stage->mirror_pose);
  add_track_behind_to_view(&stage->mirror_view, &stage->track,
                           stage->car.position);
  add_scenery_and_traffic(stage, &stage->mirror_view);
  finish_road_view(&stage->mirror_view);
  mirror_road_view(&stage->mirror_view);
}

static void render_mirror(driving_stage_ptr stage) {
  SDL_Renderer* renderer = stage->graphics_context->renderer;
  if (!renderer) {
    return;
  }
  dashboard_state_t state = dashboard_state(&stage->car, &stage->body);
  dashboard_layout_t layout =
      dashboard_layout(stage->graphics_context->screen_width,
                       stage->graphics_context->screen_height, &state);
  view_area_t area = {layout.mirror_x - layout.mirror_width / 2,
                      layout.mirror_y - layout.mirror_height / 2,
                      layout.mirror_width, layout.mirror_height};
  SDL_Rect clip = {area.x, area.y, area.width, area.height};
  SDL_RenderSetClipRect(renderer, &clip);
  render_backdrop_in(stage->graphics_context, &stage->road_renderer,
                     &stage->mirror_view, area);
  render_road_view_in(stage->graphics_context, &stage->road_renderer,
                      &stage->mirror_view, area);
  SDL_RenderSetClipRect(renderer, NULL);
  render_mirror_frame(stage->graphics_context, &layout);
}

void render_driving_stage(driving_stage_ptr stage) {
  clear_frame(stage->graphics_context);
  prepare_driving_stage_frame(stage);
  render_backdrop(stage->graphics_context, &stage->road_renderer, &stage->view);
  render_road_view(stage->graphics_context, &stage->road_renderer,
                   &stage->view);
  render_mirror(stage);
  render_cockpit(stage->graphics_context, &stage->cockpit_renderer, &stage->car,
                 &stage->body, &stage->gear_lever);
  render_hud(stage);
  render_frame(stage->graphics_context);
}

void run_driving_stage(driving_stage_ptr stage) {
  frame_limiter_t limiter = create_frame_limiter(stage->target_fps);
  while (true) {
    double delta_time = frame_limiter_wait(&limiter);
    if (drain_events() == QUIT_EVENT || is_esc_key_pressed(stage->keyboard)) {
      return;
    }
    if (is_f11_key_pressed(stage->keyboard)) {
      toggle_fullscreen(stage->graphics_context);
    }
    if (stage->audio && is_s_key_pressed(stage->keyboard)) {
      stage->muted = !stage->muted;
      set_car_audio_muted(stage->audio, stage->muted, stage->master_volume);
    }
    car_input_t input = {.accelerate = is_up_key_held(stage->keyboard),
                         .brake = is_down_key_held(stage->keyboard),
                         .left = is_left_key_held(stage->keyboard),
                         .right = is_right_key_held(stage->keyboard)};
    advance_driving_stage(stage, input, delta_time);
    if (stage->audio) {
      update_car_audio(stage->audio, &stage->car, input);
      if (stage->crashed) {
        play_crash_sound(stage->audio);
      }
    }
    render_driving_stage(stage);
  }
}
