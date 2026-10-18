#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "car.h"
#include "car_body.h"
#include "clip.h"
#include "dashboard.h"
#include "dial_sprite.h"
#include "driving_stage.h"
#include "engine_note.h"
#include "engine_synth.h"
#include "game_constants.h"
#include "gear_gate_sprite.h"
#include "gear_lever.h"
#include "lap_timer.h"
#include "projection.h"
#include "road_texture.h"
#include "road_view.h"
#include "scenery.h"
#include "spline.h"
#include "steering_wheel_sprite.h"
#include "test_allocator.h"
#include "track.h"
#include "track_generator.h"
#include "traffic.h"
#include "vec3.h"

static bool near(double a, double b) { return fabs(a - b) < 1e-6; }

/* ---- projection ---- */

static bool near_vec(vec3_t a, vec3_t b) {
  return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

static camera_pose_t pose_at(double x, double y, double z, double yaw,
                             double pitch) {
  camera_pose_t pose = {vec3(x, y, z), yaw, pitch};
  return pose;
}

static void vectors_add_scale_and_interpolate(void) {
  vec3_t a = vec3(1.0, 2.0, 3.0);
  vec3_t b = vec3(3.0, 6.0, 9.0);
  assert(near_vec(vec3_add(a, b), vec3(4.0, 8.0, 12.0)));
  assert(near_vec(vec3_sub(b, a), vec3(2.0, 4.0, 6.0)));
  assert(near_vec(vec3_scale(a, 2.0), vec3(2.0, 4.0, 6.0)));
  assert(near_vec(vec3_lerp(a, b, 0.5), vec3(2.0, 4.0, 6.0)));
  assert(near(vec3_length(vec3(3.0, 0.0, 4.0)), 5.0));
  assert(near(vec3_distance(a, b), sqrt(4.0 + 16.0 + 36.0)));
}

static void camera_space_moves_with_the_camera_position(void) {
  camera_pose_t pose = pose_at(100.0, 50.0, 500.0, 0.0, 0.0);
  vec3_t ahead = to_camera_space(&pose, vec3(100.0, 50.0, 1500.0));
  assert(near_vec(ahead, vec3(0.0, 0.0, 1000.0)));
  vec3_t beside = to_camera_space(&pose, vec3(300.0, 70.0, 500.0));
  assert(near_vec(beside, vec3(200.0, 20.0, 0.0)));
}

static void camera_space_turns_with_yaw(void) {
  /* Yaw is measured from +z, turning right (towards +x) is positive. */
  camera_pose_t facing_right = pose_at(0.0, 0.0, 0.0, M_PI / 2.0, 0.0);
  vec3_t ahead = to_camera_space(&facing_right, vec3(1000.0, 0.0, 0.0));
  assert(near_vec(ahead, vec3(0.0, 0.0, 1000.0)));
  vec3_t to_the_left = to_camera_space(&facing_right, vec3(0.0, 0.0, 1000.0));
  assert(near_vec(to_the_left, vec3(-1000.0, 0.0, 0.0)));
}

static void camera_space_tilts_with_pitch(void) {
  /* Pitch is positive when looking up. */
  camera_pose_t looking_up = pose_at(0.0, 0.0, 0.0, 0.0, M_PI / 4.0);
  vec3_t ahead = to_camera_space(&looking_up, vec3(0.0, 0.0, 1000.0));
  assert(near(ahead.x, 0.0));
  assert(near(ahead.y, -1000.0 * sqrt(0.5)));
  assert(near(ahead.z, 1000.0 * sqrt(0.5)));
}

static void projection_puts_the_horizon_at_screen_centre(void) {
  camera_t camera = create_camera(100.0, 1.6);
  projected_point_t far = project(&camera, vec3(0.0, 0.0, 1e12));
  assert(near(far.x, 0.5));
  assert(near(far.y, 0.5));
  assert(far.scale < 1e-6);
  camera_pose_t level = pose_at(0.0, 0.0, 0.0, 0.0, 0.0);
  assert(near(horizon_y(&camera, &level), 0.5));
}

static void projection_brings_near_points_to_the_bottom(void) {
  camera_t camera = create_camera(100.0, 1.6);
  projected_point_t near_point = project(&camera, vec3(0.0, -1000.0, 1000.0));
  projected_point_t far_point = project(&camera, vec3(0.0, -1000.0, 4000.0));
  assert(near_point.y > far_point.y);
  assert(near(near_point.scale, far_point.scale * 4.0));
}

static void projection_shifts_sideways_and_keeps_the_aspect(void) {
  camera_t camera = create_camera(100.0, 2.0);
  projected_point_t right = project(&camera, vec3(500.0, 0.0, 2000.0));
  projected_point_t up = project(&camera, vec3(0.0, 500.0, 2000.0));
  assert(right.x > 0.5);
  assert(near(right.y, 0.5));
  assert(up.y < 0.5);
  /* The horizontal field of view is fixed; a wider screen means the same
   * world offset moves the point further vertically than horizontally. */
  assert(near((0.5 - up.y) * 1.0, (right.x - 0.5) * 2.0));
}

static void horizon_drops_down_the_screen_when_looking_up(void) {
  camera_t camera = create_camera(100.0, 1.6);
  camera_pose_t up = pose_at(0.0, 0.0, 0.0, 0.0, 0.2);
  camera_pose_t down = pose_at(0.0, 0.0, 0.0, 0.0, -0.2);
  assert(horizon_y(&camera, &up) > 0.5);
  assert(horizon_y(&camera, &down) < 0.5);
}

static void projection_reports_points_behind_the_near_plane(void) {
  assert(!is_in_front_of_camera(vec3(0.0, 0.0, CAMERA_NEAR_PLANE * 0.5)));
  assert(is_in_front_of_camera(vec3(0.0, 0.0, CAMERA_NEAR_PLANE * 2.0)));
}

/* ---- track ---- */

static void straight_segments_run_along_z(void) {
  track_t track = create_track(10);
  add_straight(&track, 4);
  assert(track.count == 4);
  assert(near(track.length, 4 * SEGMENT_LENGTH));
  for (int i = 0; i < 4; ++i) {
    const segment_t* segment = &track.segments[i];
    assert(near(segment->curve, 0.0));
    assert(near(segment->z, i * SEGMENT_LENGTH));
    assert(near_vec(segment->start, vec3(0.0, 0.0, i * SEGMENT_LENGTH)));
    assert(near_vec(segment->end, vec3(0.0, 0.0, (i + 1) * SEGMENT_LENGTH)));
    assert(near(segment->heading, 0.0));
    assert(near(segment->end_heading, 0.0));
  }
  destroy_track(&track);
}

static void curves_ease_in_hold_and_ease_out(void) {
  track_t track = create_track(20);
  add_curve(&track, 4, 3, 4, 0.03);
  assert(track.count == 11);
  assert(track.segments[0].curve < track.segments[3].curve);
  assert(near(track.segments[4].curve, 0.03));
  assert(near(track.segments[6].curve, 0.03));
  assert(track.segments[7].curve > track.segments[10].curve);
  assert(track.segments[10].curve >= 0.0);
  destroy_track(&track);
}

static void a_right_hand_quarter_turn_ends_up_facing_right(void) {
  track_t track = create_track(120);
  add_curve(&track, 0, 100, 0, M_PI / 2.0 / 100.0);
  const segment_t* last = &track.segments[track.count - 1];
  assert(near(last->end_heading, M_PI / 2.0));
  assert(last->end.x > 0.0);
  for (int i = 1; i < track.count; ++i) {
    assert(near_vec(track.segments[i].start, track.segments[i - 1].end));
    assert(near(track.segments[i].heading, track.segments[i - 1].end_heading));
    assert(near(track.segments[i].end_heading - track.segments[i].heading,
                track.segments[i].curve));
  }
  destroy_track(&track);
}

static void road_sections_chain_their_heights(void) {
  track_t track = create_track(40);
  add_road(&track, 5, 10, 5, 0.0, 3000.0);
  add_road(&track, 0, 10, 0, 0.0, -3000.0);
  assert(track.count == 30);
  assert(near(track.segments[0].start.y, 0.0));
  for (int i = 1; i < track.count; ++i) {
    assert(near(track.segments[i].start.y, track.segments[i - 1].end.y));
  }
  for (int i = 1; i < 20; ++i) {
    assert(track.segments[i].end.y >= track.segments[i - 1].end.y);
  }
  assert(near(track.segments[19].end.y, 3000.0));
  assert(near(track.segments[29].end.y, 0.0));
  destroy_track(&track);
}

static void straights_and_curves_stay_flat(void) {
  track_t track = create_track(40);
  add_straight(&track, 5);
  add_curve(&track, 2, 2, 2, 0.02);
  for (int i = 0; i < track.count; ++i) {
    assert(near(track.segments[i].start.y, 0.0));
    assert(near(track.segments[i].end.y, 0.0));
  }
  destroy_track(&track);
}

static void track_refuses_to_grow_past_capacity(void) {
  track_t track = create_track(3);
  add_straight(&track, 5);
  assert(track.count == 3);
  destroy_track(&track);
}

static void segment_lookup_wraps_around_the_track(void) {
  track_t track = create_track(10);
  add_straight(&track, 5);
  assert(segment_index_at(&track, 0.0) == 0);
  assert(segment_index_at(&track, SEGMENT_LENGTH * 1.5) == 1);
  assert(segment_index_at(&track, track.length) == 0);
  assert(segment_index_at(&track, track.length + SEGMENT_LENGTH * 2.5) == 2);
  assert(segment_index_at(&track, -SEGMENT_LENGTH * 0.5) == 4);
  destroy_track(&track);
}

static void points_along_the_track_follow_the_centreline_and_its_sides(void) {
  track_t track = create_track(120);
  add_straight(&track, 5);
  vec3_t centre = track_point_at(&track, SEGMENT_LENGTH * 1.5, 0.0);
  assert(near_vec(centre, vec3(0.0, 0.0, SEGMENT_LENGTH * 1.5)));
  vec3_t right = track_point_at(&track, SEGMENT_LENGTH * 1.5, 0.5);
  assert(
      near_vec(right, vec3(ROAD_HALF_WIDTH * 0.5, 0.0, SEGMENT_LENGTH * 1.5)));
  assert(near(track_heading_at(&track, SEGMENT_LENGTH * 1.5), 0.0));
  add_curve(&track, 0, 100, 0, M_PI / 2.0 / 100.0);
  /* Half way round the quarter turn the road faces 45 degrees right and
   * its right-hand side is further along x and less far along z. */
  double position = SEGMENT_LENGTH * 55.0;
  assert(near(track_heading_at(&track, position), M_PI / 4.0));
  vec3_t middle = track_point_at(&track, position, 0.0);
  vec3_t side = track_point_at(&track, position, 1.0);
  assert(side.x > middle.x);
  assert(side.z < middle.z);
  assert(near(vec3_distance(middle, side), ROAD_HALF_WIDTH));
  destroy_track(&track);
}

static void slope_along_the_track_is_continuous(void) {
  track_t track = create_track(60);
  add_road(&track, 5, 20, 5, 0.0, 6000.0);
  add_road(&track, 5, 20, 5, 0.0, -6000.0);
  for (int i = 1; i < track.count; ++i) {
    double before = track_slope_at(&track, i * SEGMENT_LENGTH - 1.0);
    double after = track_slope_at(&track, i * SEGMENT_LENGTH + 1.0);
    assert(fabs(before - after) < 0.01);
  }
  assert(track_slope_at(&track, SEGMENT_LENGTH * 15) > 0.1);
  assert(track_slope_at(&track, SEGMENT_LENGTH * 45) < -0.1);
  destroy_track(&track);
}

static void rumble_stripes_alternate_every_rumble_length(void) {
  assert(is_light_segment(0));
  assert(is_light_segment(RUMBLE_LENGTH - 1));
  assert(!is_light_segment(RUMBLE_LENGTH));
  assert(is_light_segment(2 * RUMBLE_LENGTH));
}

static track_t oval_track(void) {
  track_t track = create_track(MAX_TRACK_SEGMENTS);
  add_straight(&track, 150);
  add_curve(&track, 30, 120, 30, 0.02);
  add_straight(&track, 150);
  add_curve(&track, 30, 120, 30, 0.02);
  return track;
}

static void oval_track_bends_the_same_way_twice(void) {
  track_t track = oval_track();
  assert(track.count > 0);
  int bends = 0;
  bool bending = false;
  for (int i = 0; i < track.count; ++i) {
    bool now = track.segments[i].curve > 0.0;
    assert(track.segments[i].curve >= 0.0);
    if (now && !bending) {
      ++bends;
    }
    bending = now;
  }
  assert(bends == 2);
  assert(outstanding_allocations() == 1);
  destroy_track(&track);
  assert(outstanding_allocations() == 0);
}

static void a_closed_track_joins_its_last_segment_to_its_first(void) {
  /* A square, anticlockwise seen from above: four left turns. */
  vec3_t corners[4] = {vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 1000.0),
                       vec3(-1000.0, 100.0, 1000.0), vec3(-1000.0, 0.0, 0.0)};
  track_t track = create_track(4);
  build_closed_track(&track, corners, 4);
  assert(track.count == 4);
  assert(near(track.length, 4 * SEGMENT_LENGTH));
  assert(near_vec(track.segments[3].end, track.segments[0].start));
  double total_turn = 0.0;
  for (int i = 0; i < 4; ++i) {
    const segment_t* segment = &track.segments[i];
    assert(near_vec(segment->end, track.segments[(i + 1) % 4].start));
    assert(segment->curve < 0.0);
    assert(near(segment->curve, segment->end_heading - segment->heading));
    assert(near(segment->z, i * SEGMENT_LENGTH));
    total_turn += segment->curve;
  }
  assert(near(total_turn, -2.0 * M_PI));
  /* The heading at a corner bisects the segments either side of it. */
  assert(near(track.segments[1].heading, -M_PI / 4.0));
  assert(near(track_height_at(&track, SEGMENT_LENGTH * 1.5), 50.0));
  assert(near(segment_slope(&track.segments[1]), 100.0 / SEGMENT_LENGTH));
  destroy_track(&track);
}

/* ---- spline ---- */

static void spline_passes_through_its_control_points(void) {
  vec3_t points[4] = {vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 1000.0),
                      vec3(-1000.0, 300.0, 1000.0), vec3(-1000.0, 0.0, 0.0)};
  for (int i = 0; i < 4; ++i) {
    assert(near_vec(closed_spline_point(points, 4, i), points[i]));
  }
  assert(near_vec(closed_spline_point(points, 4, 4.0), points[0]));
  assert(near_vec(closed_spline_point(points, 4, 5.5),
                  closed_spline_point(points, 4, 1.5)));
}

static void spline_bulges_outwards_between_the_corners_of_a_square(void) {
  vec3_t points[4] = {vec3(0.0, 0.0, 0.0), vec3(0.0, 0.0, 1000.0),
                      vec3(-1000.0, 0.0, 1000.0), vec3(-1000.0, 0.0, 0.0)};
  vec3_t middle = closed_spline_point(points, 4, 0.5);
  assert(near(middle.z, 500.0));
  assert(middle.x > 0.0);
  assert(middle.x < 300.0);
  assert(near(middle.y, 0.0));
}

/* ---- random track ---- */

static void generated_track_is_well_formed(const track_t* track) {
  assert(track->count > 0);
  assert(track->count <= MAX_TRACK_SEGMENTS);
  assert(track->count % TRACK_SEGMENT_MULTIPLE == 0);
  assert(near_vec(track->segments[0].start, vec3(0.0, 0.0, 0.0)));
  assert(near_vec(track->segments[track->count - 1].end,
                  track->segments[0].start));
  bool bends_left = false, bends_right = false, has_hills = false;
  for (int i = 0; i < track->count; ++i) {
    const segment_t* segment = &track->segments[i];
    if (i > 0) {
      assert(near_vec(segment->start, track->segments[i - 1].end));
      assert(near(
          wrap_angle(segment->heading - track->segments[i - 1].end_heading),
          0.0));
    }
    double length = vec3_distance(segment->start, segment->end);
    assert(fabs(length - SEGMENT_LENGTH) < SEGMENT_LENGTH * 0.25);
    assert(fabs(segment->curve) <= TRACK_MAX_CURVE + 1e-9);
    assert(fabs(segment_slope(segment)) <= TRACK_MAX_SLOPE + 1e-9);
    assert(fabs(segment->end.y) <= TRACK_MAX_HEIGHT + 1e-9);
    bends_left |= segment->curve < -TRACK_MIN_BEND;
    bends_right |= segment->curve > TRACK_MIN_BEND;
    has_hills |= fabs(segment->end.y) > TRACK_MIN_HILL;
  }
  assert(bends_left && bends_right && has_hills);
}

static void generated_tracks_are_well_formed_for_many_seeds(void) {
  for (unsigned seed = 1; seed <= 40; ++seed) {
    track_t track = generate_track(seed);
    generated_track_is_well_formed(&track);
    destroy_track(&track);
    assert(outstanding_allocations() == 0);
  }
}

static void generated_track_never_comes_close_to_itself(void) {
  for (unsigned seed = 1; seed <= 8; ++seed) {
    track_t track = generate_track(seed);
    /* Segments far apart along the lap (60 segments, so a hairpin's two
     * sides are allowed) must not come within a road's width of each
     * other in plan view. */
    for (int i = 0; i < track.count; ++i) {
      for (int j = i + 60; j < track.count; ++j) {
        if (i + track.count - j < 60) {
          continue;
        }
        vec3_t a = track.segments[i].start;
        vec3_t b = track.segments[j].start;
        double plan_distance = hypot(a.x - b.x, a.z - b.z);
        assert(plan_distance > 2.5 * ROAD_HALF_WIDTH);
      }
    }
    destroy_track(&track);
  }
}

static void generated_track_depends_only_on_the_seed(void) {
  track_t a = generate_track(1974);
  track_t b = generate_track(1974);
  track_t c = generate_track(1975);
  assert(a.count == b.count);
  assert(memcmp(a.segments, b.segments, a.count * sizeof(segment_t)) == 0);
  bool differs = a.count != c.count;
  for (int i = 0; !differs && i < a.count; ++i) {
    differs = !near_vec(a.segments[i].start, c.segments[i].start);
  }
  assert(differs);
  destroy_track(&a);
  destroy_track(&b);
  destroy_track(&c);
}

/* ---- clipping ---- */

static clip_vertex_t clip_vertex(double x, double y, double z, double u,
                                 double v) {
  clip_vertex_t vertex = {vec3(x, y, z), u, v};
  return vertex;
}

static void polygons_in_front_of_the_near_plane_are_untouched(void) {
  clip_vertex_t quad[4] = {
      clip_vertex(-1, 0, 100, 0, 0), clip_vertex(1, 0, 100, 1, 0),
      clip_vertex(1, 0, 300, 1, 1), clip_vertex(-1, 0, 300, 0, 1)};
  clip_vertex_t out[CLIP_MAX_VERTICES];
  int count = clip_to_near_plane(quad, 4, 50.0, out);
  assert(count == 4);
  for (int i = 0; i < 4; ++i) {
    assert(near_vec(out[i].position, quad[i].position));
    assert(near(out[i].u, quad[i].u) && near(out[i].v, quad[i].v));
  }
}

static void polygons_behind_the_near_plane_vanish(void) {
  clip_vertex_t quad[4] = {
      clip_vertex(-1, 0, -300, 0, 0), clip_vertex(1, 0, -300, 1, 0),
      clip_vertex(1, 0, -100, 1, 1), clip_vertex(-1, 0, -100, 0, 1)};
  clip_vertex_t out[CLIP_MAX_VERTICES];
  assert(clip_to_near_plane(quad, 4, 50.0, out) == 0);
}

static void a_straddling_polygon_is_cut_along_the_near_plane(void) {
  clip_vertex_t quad[4] = {
      clip_vertex(-1, 0, -50, 0, 0), clip_vertex(1, 0, -50, 1, 0),
      clip_vertex(1, 0, 150, 1, 1), clip_vertex(-1, 0, 150, 0, 1)};
  clip_vertex_t out[CLIP_MAX_VERTICES];
  int count = clip_to_near_plane(quad, 4, 50.0, out);
  assert(count == 4);
  for (int i = 0; i < count; ++i) {
    assert(out[i].position.z >= 50.0 - 1e-9);
    assert(out[i].v >= 0.5 - 1e-9);
  }
  /* One corner behind the plane makes a five-sided polygon. */
  clip_vertex_t tilted[4] = {
      clip_vertex(-1, 0, -50, 0, 0), clip_vertex(1, 0, 150, 1, 0),
      clip_vertex(1, 0, 350, 1, 1), clip_vertex(-1, 0, 150, 0, 1)};
  count = clip_to_near_plane(tilted, 4, 50.0, out);
  assert(count == 5);
}

/* ---- car ---- */

static const car_input_t NO_INPUT = {false, false, false, false};
static const car_input_t THROTTLE = {true, false, false, false};
static const car_input_t BRAKE = {false, true, false, false};
static const car_input_t LEFT = {false, false, true, false};
static const car_input_t RIGHT = {false, false, false, true};
static const road_conditions_t FLAT = {0.0, 0.0, false};

static void drive(car_t* car, car_input_t input, double seconds,
                  road_conditions_t road) {
  const double step = 1.0 / 240.0;
  for (double t = 0.0; t < seconds - 1e-9; t += step) {
    update_car(car, input, step, 1e9, road);
  }
}

static car_t car_at_speed(double speed) {
  car_t car = create_car();
  car.speed = speed;
  return car;
}

static void car_accelerates_from_rest(void) {
  car_t car = create_car();
  drive(&car, THROTTLE, 1.0, FLAT);
  assert(car.speed > 0.0);
  assert(car.position > 0.0);
}

static void acceleration_fades_as_speed_rises(void) {
  car_t car = create_car();
  drive(&car, THROTTLE, 1.0, FLAT);
  double first_second = car.speed;
  drive(&car, THROTTLE, 4.0, FLAT);
  double before_fifth = car.speed;
  drive(&car, THROTTLE, 1.0, FLAT);
  assert(car.speed - before_fifth < first_second * 0.5);
}

static void top_speed_is_reached_on_the_flat_and_never_exceeded(void) {
  car_t car = create_car();
  drive(&car, THROTTLE, 60.0, FLAT);
  assert(car.speed <= CAR_MAX_SPEED + 1e-6);
  assert(car.speed > CAR_MAX_SPEED * 0.99);
}

static void braking_beats_coasting_and_both_stop_the_car(void) {
  car_t coasting = car_at_speed(CAR_MAX_SPEED);
  car_t braking = car_at_speed(CAR_MAX_SPEED);
  drive(&coasting, NO_INPUT, 1.0, FLAT);
  drive(&braking, BRAKE, 1.0, FLAT);
  assert(coasting.speed < CAR_MAX_SPEED);
  assert(braking.speed < coasting.speed * 0.5);
  drive(&coasting, NO_INPUT, 60.0, FLAT);
  assert(near(coasting.speed, 0.0));
}

static void uphill_slows_a_coasting_car_faster_than_the_flat(void) {
  car_t flat = car_at_speed(CAR_MAX_SPEED / 2);
  car_t uphill = car_at_speed(CAR_MAX_SPEED / 2);
  road_conditions_t climb = {0.0, 0.3, false};
  drive(&flat, NO_INPUT, 1.0, FLAT);
  drive(&uphill, NO_INPUT, 1.0, climb);
  assert(uphill.speed < flat.speed);
}

static void flat_out_up_the_steepest_hill_keeps_most_of_the_speed(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED);
  road_conditions_t steepest = {0.0, TRACK_MAX_SLOPE, false};
  drive(&car, THROTTLE, 10.0, steepest);
  assert(car.speed >= 0.85 * CAR_MAX_SPEED);
}

static void a_parked_car_rolls_down_a_hill(void) {
  car_t car = create_car();
  road_conditions_t descent = {0.0, -0.3, false};
  drive(&car, NO_INPUT, 2.0, descent);
  assert(car.speed > 0.0);
}

static void downhill_exceeds_the_flat_top_speed_but_not_the_limit(void) {
  car_t car = create_car();
  road_conditions_t descent = {0.0, -0.4, false};
  drive(&car, THROTTLE, 60.0, descent);
  assert(car.speed > CAR_MAX_SPEED);
  assert(car.speed <= CAR_SPEED_LIMIT + 1e-6);
}

static void car_position_wraps_at_the_end_of_the_track(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED);
  car.position = 900.0;
  update_car(&car, NO_INPUT, 1.0 / 60.0, 1000.0, FLAT);
  assert(car.position < 1000.0);
  assert(car.position >= 0.0);
}

static void a_parked_car_does_not_steer(void) {
  car_t car = create_car();
  drive(&car, LEFT, 1.0, FLAT);
  assert(near(car.x, 0.0));
}

static void steering_builds_up_and_carries_on_after_release(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED);
  drive(&car, LEFT, 0.5, FLAT);
  assert(car.x < 0.0);
  assert(car.lateral_speed < 0.0);
  double at_release = car.x;
  drive(&car, NO_INPUT, 0.05, FLAT);
  assert(car.x < at_release);
  drive(&car, NO_INPUT, 2.0, FLAT);
  assert(fabs(car.lateral_speed) < 1e-3);
}

static void steering_is_measured_rather_than_twitchy(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED);
  drive(&car, RIGHT, 0.1, FLAT);
  assert(car.lateral_speed < 0.5 * CAR_STEERING_RATE);
  drive(&car, RIGHT, 0.9, FLAT);
  assert(car.x > 0.8 && car.x < 1.3);
}

static void car_cannot_leave_the_verge(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED);
  drive(&car, RIGHT, 20.0, FLAT);
  assert(near(car.x, CAR_MAX_X));
}

static void bends_push_harder_at_higher_speed(void) {
  road_conditions_t bend = {4.0, 0.0, false};
  car_t fast = car_at_speed(CAR_MAX_SPEED);
  car_t slow = car_at_speed(CAR_MAX_SPEED / 2);
  drive(&fast, NO_INPUT, 1.0, bend);
  drive(&slow, NO_INPUT, 1.0, bend);
  assert(fast.x < 0.0);
  assert(slow.x < 0.0);
  assert(fast.x < slow.x);
}

static void grass_slows_the_car_and_loosens_its_grip(void) {
  car_t on_road = car_at_speed(CAR_MAX_SPEED);
  car_t on_grass = car_at_speed(CAR_MAX_SPEED);
  on_grass.x = 1.5;
  drive(&on_road, THROTTLE, 1.0, FLAT);
  drive(&on_grass, THROTTLE, 1.0, FLAT);
  assert(on_grass.speed < on_road.speed);
  assert(on_grass.speed >= CAR_OFF_ROAD_LIMIT);
  car_t steering_on_road = car_at_speed(CAR_MAX_SPEED / 2);
  car_t steering_on_grass = car_at_speed(CAR_MAX_SPEED / 2);
  steering_on_grass.x = -1.5;
  drive(&steering_on_road, RIGHT, 0.1, FLAT);
  drive(&steering_on_grass, RIGHT, 0.1, FLAT);
  assert(steering_on_grass.lateral_speed < steering_on_road.lateral_speed);
}

static void car_ignores_bad_time_steps(void) {
  car_t car = create_car();
  update_car(&car, THROTTLE, -1.0, 1e9, FLAT);
  update_car(&car, THROTTLE, NAN, 1e9, FLAT);
  assert(near(car.speed, 0.0));
}

static void an_airborne_car_neither_drives_nor_steers(void) {
  road_conditions_t in_the_air = {0.0, 0.0, true};
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  double speed_before = car.speed;
  drive(&car, THROTTLE, 0.5, in_the_air);
  assert(car.speed < speed_before);
  car_input_t throttle_and_left = {true, false, true, false};
  drive(&car, throttle_and_left, 0.5, in_the_air);
  assert(near(car.x, 0.0));
}

static void car_remembers_its_last_acceleration(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  drive(&car, THROTTLE, 0.1, FLAT);
  assert(car.acceleration > 0.0);
  drive(&car, BRAKE, 0.1, FLAT);
  assert(car.acceleration < 0.0);
}

/* ---- car body ---- */

/* A road that climbs at a steady slope and then goes flat: a sharp crest. */
static double crest_road_height(double position) {
  const double crest_at = 40 * SEGMENT_LENGTH;
  return position < crest_at ? position * 0.4 : crest_at * 0.4;
}

static double crest_road_slope(double position) {
  return position < 40 * SEGMENT_LENGTH ? 0.4 : 0.0;
}

typedef struct {
  bool took_off;
  double highest_altitude;
  double landing_position;
  car_body_t body;
} crest_run_t;

static crest_run_t drive_over_the_crest(double speed) {
  crest_run_t run = {false, 0.0, 0.0, create_car_body(crest_road_height(0.0))};
  car_t car = car_at_speed(speed);
  const double step = 1.0 / 240.0;
  for (double t = 0.0; t < 6.0; t += step) {
    car.position += car.speed * step;
    update_car_body(&run.body, &car, step, crest_road_height(car.position),
                    crest_road_slope(car.position));
    if (run.body.airborne) {
      run.took_off = true;
      run.highest_altitude = fmax(run.highest_altitude, run.body.altitude);
    } else if (run.took_off && run.landing_position == 0.0) {
      run.landing_position = car.position;
    }
  }
  return run;
}

static void body_rides_on_the_road_surface(void) {
  car_body_t body = create_car_body(0.0);
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  for (int i = 0; i < 240; ++i) {
    car.position += car.speed / 240.0;
    update_car_body(&body, &car, 1.0 / 240.0, crest_road_height(car.position),
                    crest_road_slope(car.position));
    assert(!body.airborne);
    assert(near(body.altitude, 0.0));
    assert(near(body.height, crest_road_height(car.position)));
  }
}

static void a_fast_car_flies_over_a_sharp_crest_and_lands(void) {
  crest_run_t run = drive_over_the_crest(CAR_MAX_SPEED);
  assert(run.took_off);
  assert(run.highest_altitude > CAMERA_HEIGHT / 4);
  assert(run.landing_position > 40 * SEGMENT_LENGTH);
  assert(run.landing_position < 140 * SEGMENT_LENGTH);
  assert(!run.body.airborne);
  assert(near(run.body.altitude, 0.0));
}

static void a_slow_car_keeps_its_wheels_on_the_crest(void) {
  crest_run_t run = drive_over_the_crest(CAR_MAX_SPEED / 8);
  assert(!run.took_off);
}

static void landing_compresses_the_suspension_which_then_settles(void) {
  car_body_t body = create_car_body(0.0);
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  body.airborne = true;
  body.altitude = 2000.0;
  body.height = 2000.0;
  body.vertical_speed = 0.0;
  double deepest = 0.0;
  const double step = 1.0 / 240.0;
  for (double t = 0.0; t < 3.0; t += step) {
    update_car_body(&body, &car, step, 0.0, 0.0);
    deepest = fmax(deepest, body.suspension);
  }
  assert(!body.airborne);
  assert(deepest > 0.1);
  assert(fabs(body.suspension) < 0.01);
  assert(fabs(body.suspension_speed) < 0.05);
}

static void grass_shakes_the_body_and_tarmac_does_not(void) {
  car_body_t on_road = create_car_body(0.0);
  car_body_t on_grass = create_car_body(0.0);
  car_t road_car = car_at_speed(CAR_MAX_SPEED / 2);
  car_t grass_car = car_at_speed(CAR_MAX_SPEED / 2);
  grass_car.x = 1.5;
  double road_shake = 0.0, grass_shake = 0.0;
  const double step = 1.0 / 240.0;
  for (double t = 0.0; t < 2.0; t += step) {
    road_car.position += road_car.speed * step;
    grass_car.position += grass_car.speed * step;
    update_car_body(&on_road, &road_car, step, 0.0, 0.0);
    update_car_body(&on_grass, &grass_car, step, 0.0, 0.0);
    road_shake = fmax(road_shake, fabs(on_road.suspension));
    grass_shake = fmax(grass_shake, fabs(on_grass.suspension));
  }
  assert(road_shake < 0.01);
  assert(grass_shake > 0.05);
}

static void body_squats_under_power_and_dives_under_braking(void) {
  car_body_t body = create_car_body(0.0);
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  car.acceleration = CAR_ENGINE_ACCELERATION;
  for (int i = 0; i < 120; ++i) {
    update_car_body(&body, &car, 1.0 / 240.0, 0.0, 0.0);
  }
  assert(body.pitch > 0.3);
  car.acceleration = CAR_BRAKING_DECELERATION;
  for (int i = 0; i < 120; ++i) {
    update_car_body(&body, &car, 1.0 / 240.0, 0.0, 0.0);
  }
  assert(body.pitch < -0.3);
  assert(body.pitch >= -1.0);
}

static void a_jolt_kicks_the_suspension_and_dies_away(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  car_body_t body = create_car_body(0.0);
  jolt_car_body(&body, 1.0);
  double peak = 0.0;
  for (int step = 0; step < 240; ++step) {
    update_car_body(&body, &car, 1.0 / 240, 0.0, 0.0);
    peak = fmax(peak, fabs(body.suspension));
  }
  assert(peak > 0.2);
  assert(fabs(body.suspension) < 0.05);
}

static void body_rolls_away_from_the_direction_of_travel(void) {
  car_body_t body = create_car_body(0.0);
  car_t car = car_at_speed(CAR_MAX_SPEED);
  car.lateral_speed = -CAR_STEERING_RATE;
  for (int i = 0; i < 120; ++i) {
    update_car_body(&body, &car, 1.0 / 240.0, 0.0, 0.0);
  }
  assert(body.roll > 0.3);
  assert(body.roll <= 1.0);
  car.lateral_speed = 0.0;
  for (int i = 0; i < 480; ++i) {
    update_car_body(&body, &car, 1.0 / 240.0, 0.0, 0.0);
  }
  assert(fabs(body.roll) < 0.05);
}

/* ---- dashboard ---- */

static void needles_rest_at_zero_and_sweep_with_speed_and_revs(void) {
  car_t parked = create_car();
  car_body_t still = create_car_body(0.0);
  dashboard_state_t rest = dashboard_state(&parked, &still);
  assert(near(rest.speed_needle, 0.0));
  assert(near(rest.rev_needle, 0.0));
  assert(near(rest.wheel_angle, 0.0));
  car_t flat_out = car_at_speed(CAR_MAX_SPEED);
  dashboard_state_t fast = dashboard_state(&flat_out, &still);
  assert(fast.speed_needle > 0.9 * DIAL_SWEEP);
  assert(fast.speed_needle <= DIAL_SWEEP + 1e-9);
  car_t downhill = car_at_speed(CAR_SPEED_LIMIT);
  dashboard_state_t pinned = dashboard_state(&downhill, &still);
  assert(near(pinned.speed_needle, DIAL_SWEEP));
  car_t half = car_at_speed(CAR_MAX_SPEED / 2);
  dashboard_state_t cruising = dashboard_state(&half, &still);
  assert(near(cruising.speed_needle, fast.speed_needle / 2.0));
  assert(near(cruising.rev_needle,
              engine_rpm_fraction(CAR_MAX_SPEED / 2) * DIAL_SWEEP));
}

static void steering_wheel_turns_the_way_the_car_steers(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  car_body_t body = create_car_body(0.0);
  car.lateral_speed = CAR_STEERING_RATE / 2;
  dashboard_state_t right = dashboard_state(&car, &body);
  assert(right.wheel_angle > 0.0);
  assert(right.wheel_angle <= WHEEL_MAX_ANGLE);
  car.lateral_speed = -CAR_STEERING_RATE;
  dashboard_state_t left = dashboard_state(&car, &body);
  assert(near(left.wheel_angle, -WHEEL_MAX_ANGLE));
  car.lateral_speed = -3.0 * CAR_STEERING_RATE;
  dashboard_state_t pinned = dashboard_state(&car, &body);
  assert(near(pinned.wheel_angle, -WHEEL_MAX_ANGLE));
}

static void dashboard_moves_with_the_body(void) {
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  car_body_t body = create_car_body(0.0);
  dashboard_state_t still = dashboard_state(&car, &body);
  assert(near(still.bob, 0.0) && near(still.sway, 0.0));
  body.suspension = 0.5;
  body.roll = -0.5;
  dashboard_state_t moving = dashboard_state(&car, &body);
  assert(moving.bob > 0.0);
  assert(moving.sway < 0.0);
}

static void dial_needles_start_bottom_left_and_sweep_clockwise(void) {
  /* Screen angles: y grows downwards, so sin > 0 points down. */
  assert(cos(dial_angle(0.0)) < -0.5 && sin(dial_angle(0.0)) > 0.4);
  assert(near(sin(dial_angle(0.5)), -1.0));
  assert(cos(dial_angle(1.0)) > 0.5 && sin(dial_angle(1.0)) > 0.4);
  for (double f = 0.0; f < 1.0; f += 0.05) {
    assert(dial_angle(f + 0.05) > dial_angle(f));
  }
  assert(near(dial_angle(1.0) - dial_angle(0.0), DIAL_SWEEP));
}

static void gear_lever_sits_in_the_h_pattern_slot_of_the_gear(void) {
  gear_slot_t first = gear_gate_slot(1);
  assert(first.column == 0 && first.row == -1);
  gear_slot_t second = gear_gate_slot(2);
  assert(second.column == 0 && second.row == 1);
  gear_slot_t third = gear_gate_slot(3);
  assert(third.column == 1 && third.row == -1);
  gear_slot_t sixth = gear_gate_slot(6);
  assert(sixth.column == 2 && sixth.row == 1);
  gear_slot_t seventh = gear_gate_slot(ENGINE_GEARS);
  assert(seventh.column == 3 && seventh.row == -1);
  assert(GEAR_GATE_COLUMNS == 4);
  car_t car = car_at_speed(CAR_MAX_SPEED / 2);
  car_body_t body = create_car_body(0.0);
  dashboard_state_t state = dashboard_state(&car, &body);
  assert(state.gear == engine_gear(car.speed));
}

static void check_layout_fits(int width, int height) {
  dashboard_state_t still = {0.0, 0.0, 0.0, 0.0, 0.0, 1};
  dashboard_layout_t layout = dashboard_layout(width, height, &still);
  assert(layout.gate_x - layout.gate_width / 2 >
         layout.rev_x + layout.dial_radius);
  assert(layout.gate_x + layout.gate_width / 2 < width);
  assert(layout.gate_y - layout.gate_height / 2 >= layout.panel_top);
  assert(layout.gate_y + layout.gate_height / 2 <= height);
  assert(layout.gate_width > 0 && layout.gate_height > 0);
  assert(layout.dial_radius >= height * 12 / 100);
  int gap_left = layout.wheel_x - layout.wheel_radius -
                 (layout.speed_x + layout.dial_radius);
  int gap_right = layout.rev_x - layout.dial_radius -
                  (layout.wheel_x + layout.wheel_radius);
  assert(gap_left > 0 && gap_left <= height * 3 / 100);
  assert(gap_right > 0 && gap_right <= height * 3 / 100);
  int gap_gate = layout.gate_x - layout.gate_width / 2 -
                 (layout.rev_x + layout.dial_radius);
  assert(gap_gate > 0 && gap_gate <= height * 3 / 100);
  assert(layout.mirror_width > 0 && layout.mirror_height > 0);
  assert(layout.mirror_x == width / 2);
  assert(layout.mirror_y - layout.mirror_height / 2 >= 0);
  assert(layout.mirror_y + layout.mirror_height / 2 < height / 5);
  assert(layout.panel_top >= height * 3 / 4);
  assert(layout.wheel_radius >= height * 29 / 100);
  assert(layout.wheel_y - layout.wheel_radius >= height * 55 / 100);
  assert(layout.wheel_y - layout.wheel_radius <= height * 65 / 100);
  assert(layout.wheel_y + layout.wheel_radius > height);
  assert(layout.wheel_x == width / 2);
  assert(layout.speed_x + layout.dial_radius <
         layout.wheel_x - layout.wheel_radius);
  assert(layout.rev_x - layout.dial_radius >
         layout.wheel_x + layout.wheel_radius);
  assert(layout.speed_x - layout.dial_radius >= 0);
  assert(layout.rev_x + layout.dial_radius <= width);
  assert(layout.dial_y + layout.dial_radius <= height);
  /* Instrument pods may rise a little above the band, like real ones. */
  assert(layout.dial_y - layout.dial_radius >=
         layout.panel_top - layout.dial_radius / 2);
}

static void dashboard_sits_low_and_leaves_the_road_visible(void) {
  check_layout_fits(1600, 900);
  check_layout_fits(1280, 800);
  check_layout_fits(2560, 1440);
}

static void dashboard_layout_moves_with_the_body(void) {
  dashboard_state_t still = {0.0, 0.0, 0.0, 0.0, 0.0, 1};
  dashboard_state_t moving = {0.0, 0.0, 0.0, 10.0, -6.0, 1};
  dashboard_layout_t at_rest = dashboard_layout(1600, 900, &still);
  dashboard_layout_t shaken = dashboard_layout(1600, 900, &moving);
  assert(shaken.panel_top == at_rest.panel_top + 10);
  assert(shaken.wheel_y == at_rest.wheel_y + 10);
  assert(shaken.wheel_x == at_rest.wheel_x - 6);
  assert(shaken.speed_x == at_rest.speed_x - 6);
  dashboard_layout_t tall = dashboard_layout(3200, 1800, &moving);
  assert(tall.panel_top == 2 * at_rest.panel_top + 20);
}

static void speed_reads_in_miles_per_hour(void) {
  char text[16];
  assert(near(speed_in_mph(0.0), 0.0));
  assert(near(speed_in_mph(CAR_MAX_SPEED), CAR_TOP_SPEED_MPH));
  assert(near(speed_in_mph(CAR_MAX_SPEED / 2), CAR_TOP_SPEED_MPH / 2));
  assert(near(speed_in_mph(-CAR_MAX_SPEED), 0.0));
  format_speed(0.0, text, sizeof text);
  assert(strcmp(text, "0 MPH") == 0);
  format_speed(CAR_MAX_SPEED, text, sizeof text);
  assert(strcmp(text, "150 MPH") == 0);
  format_speed(CAR_MAX_SPEED * 0.504, text, sizeof text);
  assert(strcmp(text, "76 MPH") == 0);
}

/* ---- steering wheel sprite ---- */

#define WHEEL_TEST_SIZE 256

static uint8_t wheel_alpha(const uint32_t* pixels, int x, int y) {
  return (uint8_t)(pixels[y * WHEEL_TEST_SIZE + x] >> 24);
}

static double wheel_luminance(const uint32_t* pixels, int x, int y) {
  uint32_t p = pixels[y * WHEEL_TEST_SIZE + x];
  return ((p & 0xFF) + ((p >> 8) & 0xFF) + ((p >> 16) & 0xFF)) / 3.0;
}

static void steering_wheel_is_see_through_except_rim_spokes_and_hub(void) {
  static uint32_t pixels[WHEEL_TEST_SIZE * WHEEL_TEST_SIZE];
  render_steering_wheel_pixels(pixels, WHEEL_TEST_SIZE);
  int c = WHEEL_TEST_SIZE / 2;
  int h = WHEEL_TEST_SIZE / 2;
  assert(wheel_alpha(pixels, 2, 2) == 0);
  assert(wheel_alpha(pixels, c, c - (int)(0.55 * h)) == 0);
  assert(wheel_alpha(pixels, c + (int)(0.5 * h), c - (int)(0.5 * h)) == 0);
  assert(wheel_alpha(pixels, c, c - (int)(0.9 * h)) == 255);
  assert(wheel_alpha(pixels, c + (int)(0.55 * h), c) == 255);
  assert(wheel_alpha(pixels, c, c + (int)(0.55 * h)) == 255);
  assert(wheel_alpha(pixels, c, c) == 255);
}

static void steering_wheel_edges_are_smooth_and_symmetric(void) {
  static uint32_t pixels[WHEEL_TEST_SIZE * WHEEL_TEST_SIZE];
  render_steering_wheel_pixels(pixels, WHEEL_TEST_SIZE);
  int c = WHEEL_TEST_SIZE / 2;
  bool partial = false;
  for (int y = 0; y < c && wheel_alpha(pixels, c, y) < 255; ++y) {
    uint8_t alpha = wheel_alpha(pixels, c, y);
    partial = partial || (alpha > 0 && alpha < 255);
  }
  assert(partial);
  for (int y = 0; y < WHEEL_TEST_SIZE; ++y) {
    for (int x = 0; x < c; ++x) {
      assert(wheel_alpha(pixels, x, y) ==
             wheel_alpha(pixels, WHEEL_TEST_SIZE - 1 - x, y));
    }
  }
}

static void steering_wheel_is_lit_from_the_top_left(void) {
  static uint32_t pixels[WHEEL_TEST_SIZE * WHEEL_TEST_SIZE];
  render_steering_wheel_pixels(pixels, WHEEL_TEST_SIZE);
  int c = WHEEL_TEST_SIZE / 2;
  int reach = (int)(0.9 * (WHEEL_TEST_SIZE / 2) / sqrt(2.0));
  double lit = wheel_luminance(pixels, c - reach, c - reach);
  double shaded = wheel_luminance(pixels, c + reach, c + reach);
  assert(lit > shaded * 1.3);
  assert(wheel_luminance(pixels, c + (int)(0.55 * (WHEEL_TEST_SIZE / 2)), c) >
         wheel_luminance(pixels, c, c - (int)(0.9 * (WHEEL_TEST_SIZE / 2))));
}

/* ---- gear lever ---- */

static void gear_lever_starts_in_its_gear_and_stays_put(void) {
  gear_lever_t lever = create_gear_lever(3);
  assert(near(lever.column, 1.0) && near(lever.row, -1.0));
  update_gear_lever(&lever, 3, 1.0);
  assert(near(lever.column, 1.0) && near(lever.row, -1.0));
}

static void gear_lever_slides_straight_down_from_first_to_second(void) {
  gear_lever_t lever = create_gear_lever(1);
  update_gear_lever(&lever, 2, 0.02);
  assert(near(lever.column, 0.0));
  assert(lever.row > -1.0 && lever.row < 1.0);
  update_gear_lever(&lever, 2, 1.0);
  assert(near(lever.column, 0.0) && near(lever.row, 1.0));
}

static void gear_lever_follows_the_h_through_the_neutral_bar(void) {
  gear_lever_t lever = create_gear_lever(2);
  update_gear_lever(&lever, 3, 0.02);
  assert(near(lever.column, 0.0));
  assert(lever.row < 1.0);
  bool crossed_the_bar = false;
  for (int step = 0; step < 200; ++step) {
    update_gear_lever(&lever, 3, 0.005);
    if (lever.column > 0.05 && lever.column < 0.95) {
      assert(near(lever.row, 0.0));
      crossed_the_bar = true;
    }
  }
  assert(crossed_the_bar);
  assert(near(lever.column, 1.0) && near(lever.row, -1.0));
}

/* ---- gear gate sprites ---- */

static uint32_t sprite_pixel(const uint32_t* pixels, int width, double fx,
                             double fy, int height) {
  return pixels[(int)(fy * height) * width + (int)(fx * width)];
}

static double pixel_luminance(uint32_t p) {
  return ((p & 0xFF) + ((p >> 8) & 0xFF) + ((p >> 16) & 0xFF)) / 3.0;
}

static void gear_gate_plate_is_metal_with_dark_slots(void) {
  enum { W = 256, H = 192 };
  static uint32_t pixels[W * H];
  render_gear_gate_pixels(pixels, W, H);
  assert((pixels[0] >> 24) == 0);
  uint32_t plate = sprite_pixel(pixels, W, 0.5, 0.06, H);
  assert((plate >> 24) == 255);
  assert(pixel_luminance(plate) > 110.0);
  uint32_t slot = sprite_pixel(pixels, W, gate_slot_x(1), gate_slot_y(-1), H);
  assert((slot >> 24) == 255);
  assert(pixel_luminance(slot) < pixel_luminance(plate) * 0.5);
  uint32_t bar = sprite_pixel(pixels, W, 0.5, 0.5, H);
  assert(pixel_luminance(bar) < pixel_luminance(plate) * 0.5);
  assert(gate_slot_x(0) < gate_slot_x(3) && gate_slot_y(-1) < gate_slot_y(1));
}

static void gear_lever_has_a_knob_on_a_shaft_and_nothing_beside_it(void) {
  enum { W = 80, H = 160 };
  static uint32_t pixels[W * H];
  render_gear_lever_pixels(pixels, W, H);
  assert((pixels[0] >> 24) == 0);
  assert((sprite_pixel(pixels, W, 0.5, LEVER_KNOB_Y, H) >> 24) == 255);
  assert((sprite_pixel(pixels, W, 0.5, 0.6, H) >> 24) == 255);
  assert((sprite_pixel(pixels, W, 0.9, 0.6, H) >> 24) == 0);
  assert((sprite_pixel(pixels, W, 0.5, 0.97, H) >> 24) > 0);
}

/* ---- dial sprites ---- */

static uint32_t face_pixel_at(const uint32_t* pixels, int size, double fraction,
                              double radius) {
  double angle = dial_angle(fraction);
  int x = (int)(size / 2.0 + radius * size / 2.0 * cos(angle));
  int y = (int)(size / 2.0 + radius * size / 2.0 * sin(angle));
  return pixels[y * size + x];
}

static void dial_face_has_a_chrome_bezel_ticks_and_a_red_zone(void) {
  enum { S = 256 };
  static uint32_t rev[S * S];
  static uint32_t speed[S * S];
  render_dial_face_pixels(rev, S, 8, 0.85);
  render_dial_face_pixels(speed, S, 6, 2.0);
  assert((rev[0] >> 24) == 0);
  assert((rev[(S / 2) * S + S / 2] >> 24) == 255);
  assert(pixel_luminance(rev[(S / 2) * S + S / 2]) < 60.0);
  assert(pixel_luminance(face_pixel_at(rev, S, 0.35, 0.97)) > 140.0);
  assert(pixel_luminance(face_pixel_at(rev, S, 0.0, DIAL_TICK_RADIUS)) > 180.0);
  assert(pixel_luminance(face_pixel_at(rev, S, 0.03, DIAL_TICK_RADIUS)) < 60.0);
  uint32_t hot = face_pixel_at(rev, S, 0.95, DIAL_RED_ZONE_RADIUS);
  assert((hot & 0xFF) > 150 && ((hot >> 8) & 0xFF) < 90);
  uint32_t cool = face_pixel_at(speed, S, 0.95, DIAL_RED_ZONE_RADIUS);
  assert(((cool >> 8) & 0xFF) > (cool & 0xFF) / 2);
}

static void needle_is_a_red_blade_with_a_hub_and_nothing_beside_it(void) {
  enum { W = 32, H = 256 };
  static uint32_t pixels[W * H];
  render_needle_pixels(pixels, W, H);
  uint32_t tip = pixels[(int)(0.08 * H) * W + W / 2];
  assert((tip >> 24) == 255);
  assert((tip & 0xFF) > 180 && ((tip >> 8) & 0xFF) < 120);
  assert((pixels[(int)(0.4 * H) * W + 2] >> 24) == 0);
  uint32_t hub = pixels[(int)(NEEDLE_PIVOT_Y * H) * W + W / 2];
  assert((hub >> 24) == 255);
  assert(pixel_luminance(hub) > 90.0);
  assert((pixels[0] >> 24) == 0);
}

/* ---- road view ---- */

static road_view_t* view_for(const track_t* track, double position,
                             double car_x) {
  static road_view_t view;
  camera_t camera = create_camera(100.0, 1.6);
  camera_pose_t pose = cockpit_camera_pose(
      track, position, car_x, track_height_at(track, position), 0.0);
  begin_road_view(&view, &camera, &pose);
  add_track_to_view(&view, track, position);
  finish_road_view(&view);
  return &view;
}

static road_view_t* rear_view_for(const track_t* track, double position,
                                  double car_x) {
  static road_view_t view;
  camera_t camera = create_camera(MIRROR_FIELD_OF_VIEW_DEGREES, 3.0);
  camera_pose_t pose = mirror_camera_pose(track, position, car_x,
                                          track_height_at(track, position));
  begin_road_view(&view, &camera, &pose);
  add_track_behind_to_view(&view, track, position);
  finish_road_view(&view);
  return &view;
}

static void rear_view_looks_back_down_the_road(void) {
  track_t track = create_track(60);
  add_straight(&track, 60);
  road_view_t* rear = rear_view_for(&track, 30 * SEGMENT_LENGTH, 0.0);
  assert(rear->polygon_count > 0);
  int behind = 0, ahead = 0;
  for (int i = 0; i < rear->polygon_count; ++i) {
    if (rear->polygons[i].segment < 30) {
      ++behind;
    } else {
      ++ahead;
    }
  }
  assert(behind > 0 && ahead == 0);
  camera_pose_t pose =
      mirror_camera_pose(&track, 30 * SEGMENT_LENGTH, 0.0, 0.0);
  assert(near(wrap_angle(pose.yaw - M_PI), 0.0));
  destroy_track(&track);
}

static void mirror_shows_what_is_behind_on_the_same_side(void) {
  track_t track = create_track(60);
  add_straight(&track, 60);
  billboard_t right_behind = {track_point_at(&track, 10 * SEGMENT_LENGTH, 1.2),
                              600.0, 600.0, BILLBOARD_TREE, 0};
  road_view_t* front = view_for(&track, 30 * SEGMENT_LENGTH, 0.0);
  add_billboard_to_view(front, &right_behind);
  assert(front->sprite_count == 0);
  road_view_t* rear = rear_view_for(&track, 30 * SEGMENT_LENGTH, 0.0);
  add_billboard_to_view(rear, &right_behind);
  finish_road_view(rear);
  assert(rear->sprite_count == 1);
  assert(rear->sprites[0].x < 0.5);
  mirror_road_view(rear);
  assert(rear->sprites[0].x > 0.5);
  destroy_track(&track);
}

static double polygon_centre_x(const view_polygon_t* polygon) {
  double sum = 0.0;
  for (int i = 0; i < polygon->vertex_count; ++i) {
    sum += polygon->vertices[i].x;
  }
  return sum / polygon->vertex_count;
}

static double polygon_lowest_y(const view_polygon_t* polygon) {
  double lowest = -1e9;
  for (int i = 0; i < polygon->vertex_count; ++i) {
    lowest = fmax(lowest, polygon->vertices[i].y);
  }
  return lowest;
}

static int count_surface(const road_view_t* view, surface_t surface) {
  int count = 0;
  for (int i = 0; i < view->polygon_count; ++i) {
    count += view->polygons[i].surface == surface;
  }
  return count;
}

static void cockpit_camera_sits_above_the_car_facing_down_the_road(void) {
  track_t track = create_track(120);
  add_straight(&track, 5);
  add_curve(&track, 0, 100, 0, M_PI / 2.0 / 100.0);
  double position = SEGMENT_LENGTH * 55.0;
  camera_pose_t pose = cockpit_camera_pose(&track, position, 0.5, 300.0, 0.0);
  vec3_t under_the_car = track_point_at(&track, position, 0.5);
  assert(near(pose.position.x, under_the_car.x));
  assert(near(pose.position.z, under_the_car.z));
  assert(near(pose.position.y, 300.0 + CAMERA_HEIGHT));
  assert(near(pose.yaw, M_PI / 4.0));
  assert(near(pose.pitch, 0.0));
  destroy_track(&track);
}

static void cockpit_camera_pitches_with_the_slope_and_the_body(void) {
  track_t track = create_track(120);
  add_road(&track, 0, 100, 0, 0.0, 8000.0);
  camera_pose_t climbing =
      cockpit_camera_pose(&track, SEGMENT_LENGTH * 50, 0.0, 0.0, 0.0);
  assert(climbing.pitch > 0.0);
  camera_pose_t squatting =
      cockpit_camera_pose(&track, SEGMENT_LENGTH * 50, 0.0, 0.0, 1.0);
  assert(squatting.pitch > climbing.pitch);
  destroy_track(&track);
}

static void straight_road_stays_centred_and_narrows(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_straight(&track, DRAW_DISTANCE + 10);
  road_view_t* view = view_for(&track, 0.0, 0.0);
  assert(count_surface(view, SURFACE_ROAD) > 10);
  assert(view->polygon_count <= MAX_VIEW_POLYGONS);
  assert(near(view->horizon, 0.5));
  for (int i = 0; i < view->polygon_count; ++i) {
    const view_polygon_t* polygon = &view->polygons[i];
    if (polygon->surface != SURFACE_ROAD) {
      continue;
    }
    assert(near(polygon_centre_x(polygon), 0.5));
    assert(polygon->vertex_count >= 3);
    for (int v = 0; v < polygon->vertex_count; ++v) {
      assert(polygon->vertices[v].y >= 0.5);
    }
  }
  destroy_track(&track);
}

static void polygons_are_drawn_far_to_near(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_road(&track, 5, 20, 5, 0.0, 6000.0);
  add_road(&track, 5, 20, 5, 0.0, -6000.0);
  add_straight(&track, DRAW_DISTANCE);
  road_view_t* view = view_for(&track, 0.0, 0.0);
  assert(view->order_count == view->polygon_count);
  for (int i = 1; i < view->order_count; ++i) {
    double before = view->polygons[view->order[i - 1]].depth;
    double after = view->polygons[view->order[i]].depth;
    assert(before >= after);
  }
  destroy_track(&track);
}

static void right_hand_bend_shifts_the_distance_to_the_right(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_curve(&track, 0, DRAW_DISTANCE + 10, 0, 0.02);
  road_view_t* view = view_for(&track, 0.0, 0.0);
  /* A stretch further into the bend sits to the right of a nearer one.
   * Close to the camera the inner edge of the bend is much nearer than the
   * outer one, so the comparison is made at a distance where perspective
   * does not skew the polygon. */
  double bend_radius = SEGMENT_LENGTH / 0.02;
  const view_polygon_t* nearest = NULL;
  const view_polygon_t* far_stretch = NULL;
  const view_polygon_t* mid_stretch = NULL;
  for (int i = 0; i < view->order_count; ++i) {
    const view_polygon_t* polygon = &view->polygons[view->order[i]];
    if (polygon->surface != SURFACE_ROAD) {
      continue;
    }
    if (polygon->depth <= bend_radius && !far_stretch) {
      far_stretch = polygon;
    }
    if (polygon->depth <= bend_radius / 4.0 && !mid_stretch) {
      mid_stretch = polygon;
    }
    nearest = polygon;
  }
  assert(nearest && far_stretch && mid_stretch);
  /* The bottom of the screen is a few segments ahead of the camera. */
  assert(nearest->depth < 6.0 * SEGMENT_LENGTH);
  assert(polygon_centre_x(far_stretch) > polygon_centre_x(mid_stretch) + 0.05);
  assert(polygon_centre_x(mid_stretch) > 0.52);
  destroy_track(&track);
}

static void nothing_behind_the_camera_or_off_screen_is_kept(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_straight(&track, DRAW_DISTANCE + 10);
  road_view_t* view = view_for(&track, SEGMENT_LENGTH * 20.5, 0.0);
  int far_limit = DRAW_DISTANCE;
  assert(count_surface(view, SURFACE_ROAD) <= far_limit * 4);
  for (int i = 0; i < view->polygon_count; ++i) {
    const view_polygon_t* polygon = &view->polygons[i];
    assert(polygon->depth >= CAMERA_NEAR_PLANE);
    assert(polygon->depth <= DRAW_DISTANCE * SEGMENT_LENGTH + SEGMENT_LENGTH);
    bool all_above = true, all_below = true, all_left = true, all_right = true;
    for (int v = 0; v < polygon->vertex_count; ++v) {
      all_above &= polygon->vertices[v].y < 0.0;
      all_below &= polygon->vertices[v].y > 1.0;
      all_left &= polygon->vertices[v].x < 0.0;
      all_right &= polygon->vertices[v].x > 1.0;
    }
    assert(!all_above && !all_below && !all_left && !all_right);
  }
  destroy_track(&track);
}

static void car_offset_moves_the_road_the_other_way(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_straight(&track, DRAW_DISTANCE + 10);
  road_view_t* view = view_for(&track, 0.0, 0.5);
  const view_polygon_t* nearest =
      &view->polygons[view->order[view->order_count - 1]];
  assert(polygon_centre_x(nearest) < 0.5);
  destroy_track(&track);
}

static void road_polygons_carry_texture_rows(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_straight(&track, DRAW_DISTANCE + 10);
  road_view_t* view = view_for(&track, SEGMENT_LENGTH * 7.5, 0.0);
  for (int i = 0; i < view->polygon_count; ++i) {
    const view_polygon_t* polygon = &view->polygons[i];
    if (polygon->surface != SURFACE_ROAD) {
      continue;
    }
    double lowest_v = 2.0, highest_v = -1.0;
    for (int v = 0; v < polygon->vertex_count; ++v) {
      assert(polygon->vertices[v].u >= -1e-9 &&
             polygon->vertices[v].u <= 1.0 + 1e-9);
      assert(polygon->vertices[v].v >= -1e-9 &&
             polygon->vertices[v].v <= 1.0 + 1e-9);
      lowest_v = fmin(lowest_v, polygon->vertices[v].v);
      highest_v = fmax(highest_v, polygon->vertices[v].v);
    }
    assert(highest_v > lowest_v);
  }
  destroy_track(&track);
}

static void near_road_is_split_finer_than_far_road(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_straight(&track, DRAW_DISTANCE + 10);
  road_view_t* view = view_for(&track, 0.0, 0.0);
  int near_pieces = 0, far_pieces = 0;
  for (int i = 0; i < view->polygon_count; ++i) {
    const view_polygon_t* polygon = &view->polygons[i];
    if (polygon->surface != SURFACE_ROAD) {
      continue;
    }
    near_pieces += polygon->depth < 6.0 * SEGMENT_LENGTH;
    far_pieces += polygon->depth > 100.0 * SEGMENT_LENGTH &&
                  polygon->depth <= 101.0 * SEGMENT_LENGTH;
  }
  assert(near_pieces > 2);
  assert(far_pieces == 1);
  destroy_track(&track);
}

static void camera_rises_with_a_raised_road(void) {
  track_t flat = create_track(DRAW_DISTANCE + 10);
  add_straight(&flat, DRAW_DISTANCE + 10);
  track_t raised = create_track(DRAW_DISTANCE + 10);
  add_road(&raised, 0, 1, 0, 0.0, 5000.0);
  add_straight(&raised, DRAW_DISTANCE + 9);
  road_view_t* flat_view = view_for(&flat, SEGMENT_LENGTH * 3, 0.0);
  double flat_bottom = polygon_lowest_y(
      &flat_view->polygons[flat_view->order[flat_view->order_count - 1]]);
  road_view_t* raised_view = view_for(&raised, SEGMENT_LENGTH * 3, 0.0);
  double raised_bottom = polygon_lowest_y(
      &raised_view->polygons[raised_view->order[raised_view->order_count - 1]]);
  assert(near(flat_bottom, raised_bottom));
  destroy_track(&flat);
  destroy_track(&raised);
}

static void road_climbing_ahead_rises_on_screen_and_so_does_the_horizon(void) {
  track_t flat = create_track(DRAW_DISTANCE + 10);
  add_straight(&flat, DRAW_DISTANCE + 10);
  track_t climbing = create_track(DRAW_DISTANCE + 10);
  add_straight(&climbing, 10);
  add_road(&climbing, 20, 50, 20, 0.0, 8000.0);
  add_straight(&climbing, DRAW_DISTANCE);
  road_view_t* flat_view = view_for(&flat, 0.0, 0.0);
  double flat_far = polygon_lowest_y(&flat_view->polygons[flat_view->order[0]]);
  road_view_t* climbing_view = view_for(&climbing, 0.0, 0.0);
  double climbing_far =
      polygon_lowest_y(&climbing_view->polygons[climbing_view->order[0]]);
  assert(climbing_far < flat_far);
  assert(near(climbing_view->horizon, 0.5));
  road_view_t* on_the_hill = view_for(&climbing, SEGMENT_LENGTH * 50, 0.0);
  assert(on_the_hill->horizon > 0.5);
  destroy_track(&flat);
  destroy_track(&climbing);
}

static void billboards_stand_on_the_ground_and_sort_with_the_road(void) {
  track_t track = create_track(DRAW_DISTANCE + 10);
  add_straight(&track, DRAW_DISTANCE + 10);
  static road_view_t view;
  camera_t camera = create_camera(100.0, 1.6);
  camera_pose_t pose = cockpit_camera_pose(&track, 0.0, 0.0, 0.0, 0.0);
  begin_road_view(&view, &camera, &pose);
  add_track_to_view(&view, &track, 0.0);
  billboard_t near_right = {vec3(ROAD_HALF_WIDTH * 1.2, 0.0, 2000.0), 600.0,
                            1200.0, BILLBOARD_TREE, 0};
  billboard_t far_left = {vec3(-ROAD_HALF_WIDTH * 1.2, 0.0, 8000.0), 600.0,
                          1200.0, BILLBOARD_TREE, 1};
  billboard_t behind = {vec3(0.0, 0.0, -500.0), 600.0, 1200.0, BILLBOARD_SIGN,
                        0};
  add_billboard_to_view(&view, &near_right);
  add_billboard_to_view(&view, &far_left);
  add_billboard_to_view(&view, &behind);
  finish_road_view(&view);
  assert(view.sprite_count == 2);
  assert(view.sprites[0].x > 0.5 && view.sprites[1].x < 0.5);
  assert(view.sprites[0].y > view.sprites[1].y);
  assert(near(view.sprites[0].width, view.sprites[1].width * 4.0));
  assert(near(view.sprites[0].height / view.sprites[0].width,
              2.0 * camera.aspect));
  assert(view.sprites[0].variant == 0 && view.sprites[1].variant == 1);
  /* The near tree is drawn after every polygon farther than it and before
   * every polygon nearer than it. */
  assert(view.order_count == view.polygon_count + 2);
  bool seen_tree = false;
  for (int i = 0; i < view.order_count; ++i) {
    int id = view.order[i];
    if (id >= MAX_VIEW_POLYGONS) {
      seen_tree |= id - MAX_VIEW_POLYGONS == 0;
      continue;
    }
    if (seen_tree) {
      assert(view.polygons[id].depth <= 2000.0 + 1e-9);
    } else {
      assert(view.polygons[id].depth >= 2000.0 - 1e-9);
    }
  }
  assert(seen_tree);
  destroy_track(&track);
}

static void a_closed_circuit_is_drawn_only_within_the_draw_distance(void) {
  track_t track = generate_track(7);
  road_view_t* view = view_for(&track, 0.0, 0.0);
  assert(count_surface(view, SURFACE_ROAD) > 20);
  for (int i = 0; i < view->polygon_count; ++i) {
    assert(view->polygons[i].depth <=
           DRAW_DISTANCE * SEGMENT_LENGTH + SEGMENT_LENGTH);
    assert(view->polygons[i].fog >= 0.0 && view->polygons[i].fog <= 1.0);
  }
  destroy_track(&track);
}

/* ---- scenery ---- */

static void scenery_stands_beside_the_road_on_both_sides(void) {
  track_t track = generate_track(7);
  scenery_t scenery = place_scenery(&track, 7);
  assert(scenery.count > track.count / 8);
  assert(scenery.count <= MAX_SCENERY);
  bool left = false, right = false;
  for (int i = 0; i < scenery.count; ++i) {
    const scenery_item_t* item = &scenery.items[i];
    assert(fabs(item->lateral) >= SCENERY_NEAREST_LATERAL - 1e-9);
    assert(fabs(item->lateral) <= SCENERY_FARTHEST_LATERAL + 1e-9);
    assert(item->segment >= 0 && item->segment < track.count);
    if (i > 0) {
      assert(item->segment >= scenery.items[i - 1].segment);
    }
    assert(item->billboard.kind == BILLBOARD_TREE ||
           item->billboard.kind == BILLBOARD_BUSH ||
           item->billboard.kind == BILLBOARD_SIGN);
    assert(item->billboard.width > 0.0 && item->billboard.height > 0.0);
    vec3_t expected =
        track_point_at(&track, item->segment * SEGMENT_LENGTH, item->lateral);
    assert(near_vec(item->billboard.position, expected));
    left |= item->lateral < 0.0;
    right |= item->lateral > 0.0;
  }
  assert(left && right);
  destroy_scenery(&scenery);
  destroy_track(&track);
  assert(outstanding_allocations() == 0);
}

static void scenery_depends_only_on_the_seed(void) {
  track_t track = generate_track(3);
  scenery_t a = place_scenery(&track, 11);
  scenery_t b = place_scenery(&track, 11);
  scenery_t c = place_scenery(&track, 12);
  assert(a.count == b.count);
  assert(memcmp(a.items, b.items, a.count * sizeof(scenery_item_t)) == 0);
  bool differs = a.count != c.count;
  for (int i = 0; !differs && i < a.count; ++i) {
    differs = a.items[i].segment != c.items[i].segment ||
              !near(a.items[i].lateral, c.items[i].lateral);
  }
  assert(differs);
  destroy_scenery(&a);
  destroy_scenery(&b);
  destroy_scenery(&c);
  destroy_track(&track);
}

static void scenery_blocks_a_car_that_drives_into_it(void) {
  track_t track = create_track(40);
  add_straight(&track, 40);
  scenery_item_t tree = {
      {vec3(0.0, 0.0, 0.0), 600.0, 1200.0, BILLBOARD_TREE, 0}, 10, 1.6};
  tree.billboard.position = track_point_at(&track, 10 * SEGMENT_LENGTH, 1.6);
  scenery_t scenery = {&tree, 1};
  car_t car = create_car();
  car.position = 10 * SEGMENT_LENGTH + 20.0;
  car.x = 1.6;
  assert(scenery_blocks(&scenery, &car));
  car.x = 1.6 - CAR_HALF_WIDTH - 600.0 / ROAD_HALF_WIDTH / 2.0 - 0.01;
  assert(!scenery_blocks(&scenery, &car));
  car.x = -1.6;
  assert(!scenery_blocks(&scenery, &car));
  car.x = 1.6;
  car.position = 12 * SEGMENT_LENGTH;
  assert(!scenery_blocks(&scenery, &car));
  car.position = 9 * SEGMENT_LENGTH + 150.0;
  assert(scenery_blocks(&scenery, &car));
  destroy_track(&track);
}

/* ---- traffic ---- */

static void traffic_is_spread_along_the_lanes(void) {
  double length = 1000 * SEGMENT_LENGTH;
  traffic_t traffic = create_traffic(5, length, 12);
  assert(traffic.count == 12);
  for (int i = 0; i < traffic.count; ++i) {
    const traffic_car_t* car = &traffic.cars[i];
    assert(car->position >= 0.0 && car->position < length);
    assert(fabs(car->lateral) <= 1.0 - CAR_HALF_WIDTH);
    double slowest = car->truck ? TRAFFIC_TRUCK_MIN_SPEED : TRAFFIC_MIN_SPEED;
    double fastest = car->truck ? TRAFFIC_TRUCK_MAX_SPEED : TRAFFIC_MAX_SPEED;
    assert(car->speed >= slowest && car->speed <= fastest);
    assert(car->colour >= 0 && car->colour < BILLBOARD_VARIANTS);
    if (i > 0) {
      assert(car->position >
             traffic.cars[i - 1].position + 2 * TRAFFIC_CAR_LENGTH);
    }
  }
  traffic_t same = create_traffic(5, length, 12);
  assert(memcmp(&same, &traffic, sizeof(traffic_t)) == 0);
}

static void traffic_drives_on_and_wraps_around_the_lap(void) {
  double length = 100 * SEGMENT_LENGTH;
  traffic_t traffic = create_traffic(1, length, 3);
  double before[3];
  for (int i = 0; i < 3; ++i) {
    before[i] = traffic.cars[i].position;
  }
  for (int step = 0; step < 240; ++step) {
    update_traffic(&traffic, 1.0 / 240, length, NULL);
  }
  for (int i = 0; i < 3; ++i) {
    double expected = fmod(before[i] + traffic.cars[i].speed, length);
    assert(fabs(traffic.cars[i].position - expected) < 1e-6);
  }
  for (int step = 0; step < 240 * 60; ++step) {
    update_traffic(&traffic, 1.0 / 240, length, NULL);
  }
  for (int i = 0; i < 3; ++i) {
    assert(traffic.cars[i].position >= 0.0 &&
           traffic.cars[i].position < length);
  }
}

static void traffic_keeps_its_distance_from_the_car_ahead(void) {
  double length = 100 * SEGMENT_LENGTH;
  traffic_t traffic = {{{1000.0, 0.0, TRAFFIC_MIN_SPEED, 0, false},
                        {0.0, 0.0, TRAFFIC_MAX_SPEED, 1, false}},
                       2};
  for (int step = 0; step < 240 * 10; ++step) {
    update_traffic(&traffic, 1.0 / 240, length, NULL);
    double gap = fmod(
        traffic.cars[0].position - traffic.cars[1].position + length, length);
    assert(gap >= TRAFFIC_CAR_LENGTH);
  }
  assert(traffic.cars[1].speed <= TRAFFIC_MIN_SPEED + 1e-6);
}

static void running_into_traffic_slows_the_player_to_its_speed(void) {
  double length = 100 * SEGMENT_LENGTH;
  traffic_t traffic = {
      {{10 * SEGMENT_LENGTH, 0.0, TRAFFIC_MIN_SPEED, 0, false}}, 1};
  car_t player = car_at_speed(CAR_MAX_SPEED);
  player.position = 10 * SEGMENT_LENGTH - TRAFFIC_CAR_LENGTH / 2.0;
  player.x = 0.1;
  assert(hit_traffic(&traffic, &player, length));
  assert(player.speed <= TRAFFIC_MIN_SPEED + 1e-9);
  assert(player.lateral_speed > 0.0);
  assert(player.position < 10 * SEGMENT_LENGTH - TRAFFIC_CAR_LENGTH + 1e-9);
  car_t beside = car_at_speed(CAR_MAX_SPEED);
  beside.position = 10 * SEGMENT_LENGTH;
  beside.x = 0.8;
  assert(!hit_traffic(&traffic, &beside, length));
  assert(near(beside.speed, CAR_MAX_SPEED));
  car_t behind = car_at_speed(CAR_MAX_SPEED);
  behind.position = 10 * SEGMENT_LENGTH - 3 * TRAFFIC_CAR_LENGTH;
  assert(!hit_traffic(&traffic, &behind, length));
}

static void traffic_billboards_sit_on_the_road_in_their_lane(void) {
  track_t track = create_track(40);
  add_straight(&track, 40);
  traffic_car_t car = {5 * SEGMENT_LENGTH, -0.6, TRAFFIC_MIN_SPEED, 3, false};
  billboard_t billboard = traffic_billboard(&track, &car);
  assert(billboard.kind == BILLBOARD_CAR);
  assert(billboard.variant == 3);
  assert(
      near_vec(billboard.position, track_point_at(&track, car.position, -0.6)));
  assert(billboard.width > 0.0 && billboard.height > 0.0);
  destroy_track(&track);
}

static void traffic_includes_slower_longer_trucks(void) {
  double length = 400 * SEGMENT_LENGTH;
  traffic_t traffic = create_traffic(7, length, 16);
  int trucks = 0;
  for (int i = 0; i < traffic.count; ++i) {
    const traffic_car_t* vehicle = &traffic.cars[i];
    if (vehicle->truck) {
      ++trucks;
      assert(vehicle->speed <= TRAFFIC_TRUCK_MAX_SPEED + 1e-9);
      assert(vehicle->speed >= TRAFFIC_TRUCK_MIN_SPEED - 1e-9);
      assert(near(traffic_vehicle_length(vehicle), TRAFFIC_TRUCK_LENGTH));
      assert(near(traffic_vehicle_width(vehicle), TRAFFIC_TRUCK_WIDTH));
    } else {
      assert(near(traffic_vehicle_length(vehicle), TRAFFIC_CAR_LENGTH));
      assert(near(traffic_vehicle_width(vehicle), TRAFFIC_CAR_WIDTH));
    }
  }
  assert(trucks >= 3 && trucks < traffic.count);
  assert(!traffic.cars[0].truck);
}

static void trucks_are_drawn_bigger_and_stop_the_car_further_back(void) {
  track_t track = create_track(40);
  add_straight(&track, 40);
  traffic_car_t car = {5 * SEGMENT_LENGTH, 0.0, TRAFFIC_MIN_SPEED, 1, false};
  traffic_car_t truck = {5 * SEGMENT_LENGTH, 0.0, TRAFFIC_MIN_SPEED, 1, true};
  billboard_t car_board = traffic_billboard(&track, &car);
  billboard_t truck_board = traffic_billboard(&track, &truck);
  assert(truck_board.kind == BILLBOARD_TRUCK);
  assert(truck_board.width > car_board.width * 1.2);
  assert(truck_board.height > car_board.height * 1.8);
  assert(billboard_variants(BILLBOARD_TRUCK) >= 2);
  traffic_t traffic;
  traffic.count = 1;
  traffic.cars[0] = truck;
  car_t player = car_at_speed(CAR_MAX_SPEED);
  player.position = truck.position - 100.0;
  assert(hit_traffic(&traffic, &player, track.length));
  assert(near(player.position, truck.position - TRAFFIC_TRUCK_LENGTH));
  destroy_track(&track);
}

/* ---- laps ---- */

static void lap_timer_counts_laps_and_keeps_the_best(void) {
  double length = 100 * SEGMENT_LENGTH;
  lap_timer_t timer = create_lap_timer();
  assert(timer.laps == 0 && !timer.has_best);
  update_lap_timer(&timer, 10.0, 0.0, length * 0.5, length);
  assert(near(timer.current, 10.0));
  assert(timer.laps == 0);
  update_lap_timer(&timer, 20.0, length * 0.9, 50.0, length);
  assert(timer.laps == 1);
  assert(timer.has_best);
  assert(near(timer.best, 30.0));
  assert(near(timer.last, 30.0));
  assert(near(timer.current, 0.0));
  update_lap_timer(&timer, 25.0, 50.0, length * 0.95, length);
  update_lap_timer(&timer, 1.0, length * 0.95, 10.0, length);
  assert(timer.laps == 2);
  assert(near(timer.best, 26.0));
  assert(near(timer.last, 26.0));
  update_lap_timer(&timer, 40.0, 10.0, length * 0.99, length);
  update_lap_timer(&timer, 1.0, length * 0.99, 1.0, length);
  assert(timer.laps == 3);
  assert(near(timer.best, 26.0));
  assert(near(timer.last, 41.0));
}

static void rolling_backwards_over_the_line_does_not_count(void) {
  double length = 100 * SEGMENT_LENGTH;
  lap_timer_t timer = create_lap_timer();
  update_lap_timer(&timer, 1.0, 10.0, length - 10.0, length);
  assert(timer.laps == 0);
  assert(near(timer.current, 1.0));
}

static void lap_time_formats_as_minutes_seconds_and_tenths(void) {
  char text[16];
  format_lap_time(83.47, text, sizeof text);
  assert(strcmp(text, "1:23.4") == 0);
  format_lap_time(0.0, text, sizeof text);
  assert(strcmp(text, "0:00.0") == 0);
}

/* ---- road texture ---- */

static void asphalt_is_grey_noise_around_the_base_colour(void) {
  uint32_t* pixels = calloc(ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE, 4);
  generate_asphalt_pixels(pixels, 7);
  int distinct = 0;
  uint32_t first = pixels[0];
  for (int i = 0; i < ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE; ++i) {
    unsigned r = pixels[i] & 0xFF, g = (pixels[i] >> 8) & 0xFF,
             b = (pixels[i] >> 16) & 0xFF, a = pixels[i] >> 24;
    assert(r == g && g == b && a == 255);
    assert(abs((int)r - ROAD_TEXTURE_BASE_GREY) <= ROAD_TEXTURE_MAX_DEVIATION);
    if (pixels[i] != first) {
      ++distinct;
    }
  }
  assert(distinct > ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE / 4);
  free(pixels);
}

static void asphalt_is_deterministic_for_a_seed(void) {
  uint32_t* a = calloc(ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE, 4);
  uint32_t* b = calloc(ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE, 4);
  uint32_t* c = calloc(ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE, 4);
  generate_asphalt_pixels(a, 7);
  generate_asphalt_pixels(b, 7);
  generate_asphalt_pixels(c, 8);
  assert(memcmp(a, b, ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE * 4) == 0);
  assert(memcmp(a, c, ROAD_TEXTURE_SIZE * ROAD_TEXTURE_SIZE * 4) != 0);
  free(a);
  free(b);
  free(c);
}

/* ---- engine note ---- */

static void engine_revs_climb_through_each_gear_and_drop_at_the_change(void) {
  assert(near(engine_rpm_fraction(0.0), 0.0));
  int gear_changes = 0;
  double previous = 0.0;
  for (double speed = 0.0; speed <= CAR_SPEED_LIMIT;
       speed += CAR_SPEED_LIMIT / 2000) {
    double rpm = engine_rpm_fraction(speed);
    assert(rpm >= 0.0 && rpm <= 1.0);
    if (rpm < previous - 0.2) {
      ++gear_changes;
    } else {
      assert(rpm >= previous - 1e-9);
    }
    previous = rpm;
  }
  assert(gear_changes == ENGINE_GEARS - 1);
  assert(engine_rpm_fraction(CAR_SPEED_LIMIT) > 0.95);
}

static void gear_indicator_climbs_from_first_to_top(void) {
  assert(engine_gear(0.0) == 1);
  int previous = 1;
  for (double speed = 0.0; speed <= CAR_SPEED_LIMIT;
       speed += CAR_SPEED_LIMIT / 500) {
    int gear = engine_gear(speed);
    assert(gear >= previous && gear <= ENGINE_GEARS);
    previous = gear;
  }
  assert(engine_gear(CAR_SPEED_LIMIT) == ENGINE_GEARS);
}

static double render_seconds(engine_synth_t* synth, double rpm, bool throttle,
                             double volume, double seconds, int16_t* out,
                             int* firings_before) {
  enum { CHANNELS = 2, RATE = 44100 };
  int frames = (int)(seconds * RATE);
  *firings_before = synth->firings;
  double energy = 0.0;
  for (int done = 0; done < frames; done += 1024) {
    int chunk = frames - done < 1024 ? frames - done : 1024;
    render_engine_synth(synth, rpm, throttle, volume, out + done * CHANNELS,
                        chunk, CHANNELS);
  }
  for (int i = 0; i < frames * CHANNELS; ++i) {
    energy += (double)out[i] * out[i];
  }
  return sqrt(energy / (frames * CHANNELS));
}

static void firing_rate_follows_the_revs_continuously(void) {
  assert(engine_firing_hz(0.0) >= 20.0);
  assert(engine_firing_hz(1.0) <= 500.0);
  double previous = engine_firing_hz(0.0);
  for (double rpm = 0.01; rpm <= 1.0; rpm += 0.01) {
    double hz = engine_firing_hz(rpm);
    assert(hz > previous);
    assert(hz - previous < 10.0);
    previous = hz;
  }
}

static void engine_fires_at_the_firing_rate_with_jitter(void) {
  static int16_t out[2 * 44100 * 2];
  engine_synth_t synth = create_engine_synth(44100);
  int before;
  render_seconds(&synth, 0.5, true, 1.0, 1.0, out, &before);
  render_seconds(&synth, 0.5, true, 1.0, 2.0, out, &before);
  double fired = (synth.firings - before) / 2.0;
  double expected = engine_firing_hz(0.5);
  assert(fabs(fired - expected) < expected * 0.06);
  /* The gaps between firings are not all equal: it is a motor, not a
   * metronome. */
  assert(synth.shortest_cycle < synth.longest_cycle * 0.97);
}

static void engine_is_louder_and_harsher_under_throttle(void) {
  static int16_t out[2 * 44100];
  engine_synth_t synth = create_engine_synth(44100);
  int before;
  render_seconds(&synth, 0.6, false, 1.0, 0.5, out, &before);
  double coasting = render_seconds(&synth, 0.6, false, 1.0, 1.0, out, &before);
  render_seconds(&synth, 0.6, true, 1.0, 0.5, out, &before);
  double driving = render_seconds(&synth, 0.6, true, 1.0, 1.0, out, &before);
  assert(driving > coasting * 1.2);
  assert(driving > 2000.0);
}

static void engine_output_stays_in_range_and_obeys_the_volume(void) {
  static int16_t out[2 * 44100];
  engine_synth_t synth = create_engine_synth(44100);
  int before;
  int peak = 0;
  render_seconds(&synth, 1.0, true, 1.0, 1.0, out, &before);
  for (int i = 0; i < 2 * 44100; ++i) {
    peak = abs(out[i]) > peak ? abs(out[i]) : peak;
    if (i % 2 == 0) {
      assert(out[i] == out[i + 1]);
    }
  }
  assert(peak > 6000 && peak < 32767);
  double silent = render_seconds(&synth, 1.0, true, 0.0, 0.2, out, &before);
  assert(near(silent, 0.0));
  double quiet = render_seconds(&synth, 1.0, true, 0.25, 0.5, out, &before);
  double loud = render_seconds(&synth, 1.0, true, 1.0, 0.5, out, &before);
  assert(quiet < loud / 2.0);
}

static void revs_change_smoothly_rather_than_in_steps(void) {
  static int16_t out[2 * 44100];
  engine_synth_t synth = create_engine_synth(44100);
  int before;
  render_seconds(&synth, 0.2, true, 1.0, 1.0, out, &before);
  assert(fabs(synth.rpm - 0.2) < 0.01);
  render_seconds(&synth, 0.8, true, 1.0, 0.02, out, &before);
  assert(synth.rpm > 0.2 && synth.rpm < 0.7);
  render_seconds(&synth, 0.8, true, 1.0, 1.0, out, &before);
  assert(fabs(synth.rpm - 0.8) < 0.01);
}

static void skid_loop_is_noise_with_a_soft_join(void) {
  enum { MAX_FRAMES = 44100, CHANNELS = 2 };
  int16_t* samples = calloc(MAX_FRAMES * CHANNELS, sizeof(int16_t));
  int frames = synthesize_skid_loop(samples, MAX_FRAMES, CHANNELS, 44100);
  assert(frames > 4000 && frames <= MAX_FRAMES);
  double energy = 0.0;
  int peak = 0;
  for (int i = 0; i < frames * CHANNELS; ++i) {
    energy += (double)samples[i] * samples[i];
    peak = abs(samples[i]) > peak ? abs(samples[i]) : peak;
  }
  assert(peak > 4000 && peak < 32000);
  assert(sqrt(energy / (frames * CHANNELS)) > 1000.0);
  assert(abs(samples[(frames - 1) * CHANNELS] - samples[0]) < peak / 4);
  free(samples);
}

static void crash_thump_starts_loud_and_dies_away(void) {
  static int16_t samples[2 * 44100];
  int frames = synthesize_thump(samples, 44100, 2, 44100);
  assert(frames > 4410 && frames <= 44100);
  int loud_early = 0, loud_late = 0;
  for (int i = 0; i < frames / 10; ++i) {
    loud_early += abs(samples[i * 2]) > 3000;
    loud_late += abs(samples[(frames - 1 - i) * 2]) > 3000;
  }
  assert(loud_early > frames / 40);
  assert(loud_late == 0);
  assert(samples[(frames - 1) * 2] == 0);
}

static void engine_is_louder_under_throttle_and_skid_needs_speed(void) {
  assert(engine_volume(true, 0.5) > engine_volume(false, 0.5));
  assert(engine_volume(true, 1.0) <= 1.0);
  assert(engine_volume(false, 0.0) > 0.0);
  assert(near(skid_volume(false, CAR_MAX_SPEED), 0.0));
  assert(near(skid_volume(true, 0.0), 0.0));
  assert(skid_volume(true, CAR_MAX_SPEED) >
         skid_volume(true, CAR_MAX_SPEED / 2));
  assert(skid_volume(true, CAR_MAX_SPEED) <= 1.0);
  assert(near(grass_volume(false, CAR_MAX_SPEED), 0.0));
  assert(grass_volume(true, CAR_MAX_SPEED) > 0.0);
}

/* ---- laps at different frame rates ---- */

static double position_after_twenty_seconds(const track_t* track, int fps) {
  car_t car = create_car();
  for (int frame = 0; frame < 20 * fps; ++frame) {
    int index = segment_index_at(track, car.position);
    road_conditions_t road = {track->segments[index].curve,
                              segment_slope(&track->segments[index]), false};
    update_car(&car, THROTTLE, 1.0 / fps, track->length, road);
  }
  return car.position;
}

static void simulation_barely_depends_on_the_step_size(void) {
  track_t track = generate_track(3);
  double at_30 = position_after_twenty_seconds(&track, 30);
  double at_240 = position_after_twenty_seconds(&track, 240);
  assert(at_30 > 10 * SEGMENT_LENGTH);
  assert(fabs(at_30 - at_240) < 2 * SEGMENT_LENGTH);
  destroy_track(&track);
}

/* ---- driving stage ---- */

static driving_stage_t stage_with_dummy_screen(void) {
  static graphics_context_t graphics_context;
  graphics_context.screen_width = 1440;
  graphics_context.screen_height = 900;
  graphics_context.screen_center = point(720, 450);
  driving_stage_t stage;
  assert(
      init_driving_stage(&stage, &graphics_context, NULL, NULL, false, 60, 7));
  return stage;
}

static double position_after_two_seconds(double delta_time) {
  driving_stage_t stage = stage_with_dummy_screen();
  double simulated_frames = 0.0;
  while (simulated_frames < 120.0) {
    advance_driving_stage(&stage, THROTTLE, delta_time);
    simulated_frames += delta_time;
  }
  double position = stage.car.position;
  destroy_driving_stage(&stage);
  return position;
}

static void stage_simulates_identically_at_any_render_rate(void) {
  double at_240 = position_after_two_seconds(0.25);
  double at_60 = position_after_two_seconds(1.0);
  double at_30 = position_after_two_seconds(2.0);
  assert(near(at_240, at_60));
  assert(near(at_60, at_30));
  assert(at_60 > 0.0);
}

static void stage_caps_catch_up_after_a_stall(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  stage.car.speed = CAR_MAX_SPEED;
  advance_driving_stage(&stage, NO_INPUT, 600.0);
  double covered = stage.car.position;
  double cap = CAR_MAX_SPEED * MAX_FRAME_CATCH_UP_MS / 1000.0;
  assert(covered <= cap + 1e-6);
  assert(covered > cap / 2);
  destroy_driving_stage(&stage);
}

static int first_segment_where(const track_t* track,
                               bool (*matches)(const segment_t*)) {
  for (int i = 0; i < track->count; ++i) {
    if (matches(&track->segments[i])) {
      return i;
    }
  }
  assert(false);
  return -1;
}

static bool bends_right(const segment_t* segment) {
  return segment->curve > TRACK_MIN_BEND;
}
static bool descends(const segment_t* segment) {
  return segment_slope(segment) < -0.1;
}

static void stage_uses_the_curve_under_the_car(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  stage.car.speed = CAR_MAX_SPEED;
  stage.car.position =
      first_segment_where(&stage.track, bends_right) * SEGMENT_LENGTH;
  advance_driving_stage(&stage, NO_INPUT, 1.0);
  assert(stage.car.x < 0.0);
  destroy_driving_stage(&stage);
}

static void stage_uses_the_slope_under_the_car(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  stage.car.position =
      first_segment_where(&stage.track, descends) * SEGMENT_LENGTH;
  for (int frame = 0; frame < 60; ++frame) {
    advance_driving_stage(&stage, NO_INPUT, 1.0);
  }
  assert(stage.car.speed > 0.0);
  destroy_driving_stage(&stage);
}

static void stage_track_comes_from_the_seed(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  track_t expected = generate_track(7);
  assert(stage.track.count == expected.count);
  assert(stage.scenery.count > 0);
  assert(stage.traffic.count == TRAFFIC_COUNT);
  destroy_track(&expected);
  destroy_driving_stage(&stage);
}

static void stage_stops_the_car_behind_traffic(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  const traffic_car_t* ahead = &stage.traffic.cars[0];
  stage.car.position = ahead->position - 4 * TRAFFIC_CAR_LENGTH;
  stage.car.x = ahead->lateral;
  stage.car.speed = CAR_MAX_SPEED;
  bool crashed = false;
  for (int frame = 0; frame < 30; ++frame) {
    advance_driving_stage(&stage, THROTTLE, 1.0);
    crashed |= stage.crashed;
  }
  assert(crashed);
  assert(stage.car.speed <= TRAFFIC_MAX_SPEED + 1e-6);
  assert(fabs(stage.body.suspension) > 0.0);
  destroy_driving_stage(&stage);
}

static void stage_shows_traffic_behind_in_the_mirror(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  const traffic_car_t* behind = &stage.traffic.cars[0];
  stage.car.position = behind->position + 3 * TRAFFIC_CAR_LENGTH;
  stage.car.x = behind->lateral;
  prepare_driving_stage_frame(&stage);
  assert(stage.mirror_view.sprite_count > 0);
  assert(stage.mirror_view.polygon_count > 0);
  destroy_driving_stage(&stage);
}

static void stage_crashes_the_car_into_a_tree(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  const scenery_item_t* tree = NULL;
  for (int i = 0; i < stage.scenery.count && !tree; ++i) {
    if (fabs(stage.scenery.items[i].lateral) < CAR_MAX_X - 0.1) {
      tree = &stage.scenery.items[i];
    }
  }
  assert(tree);
  stage.car.position = tree->segment * SEGMENT_LENGTH - SEGMENT_LENGTH;
  stage.car.x = tree->lateral;
  stage.car.speed = CAR_MAX_SPEED / 2;
  advance_driving_stage(&stage, NO_INPUT, 4.0);
  assert(stage.crashed);
  assert(stage.car.speed < CAR_MAX_SPEED / 2 * CRASH_SPEED_FACTOR * 1.5);
  destroy_driving_stage(&stage);
}

static void stage_counts_laps(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  stage.car.position = stage.track.length - 10.0;
  stage.car.speed = CAR_MAX_SPEED / 4;
  stage.car.x = -1.5;
  advance_driving_stage(&stage, NO_INPUT, 1.0);
  assert(stage.lap_timer.laps == 1);
  assert(stage.lap_timer.has_best);
  destroy_driving_stage(&stage);
}

static void stage_releases_everything(void) {
  driving_stage_t stage = stage_with_dummy_screen();
  destroy_driving_stage(&stage);
  assert(outstanding_allocations() == 0);
}

typedef struct {
  const char* name;
  void (*run)(void);
} test_case_t;

int main(int argc, char* argv[]) {
  const test_case_t cases[] = {
      {"projection", vectors_add_scale_and_interpolate},
      {"projection", camera_space_moves_with_the_camera_position},
      {"projection", camera_space_turns_with_yaw},
      {"projection", camera_space_tilts_with_pitch},
      {"projection", projection_puts_the_horizon_at_screen_centre},
      {"projection", projection_brings_near_points_to_the_bottom},
      {"projection", projection_shifts_sideways_and_keeps_the_aspect},
      {"projection", horizon_drops_down_the_screen_when_looking_up},
      {"projection", projection_reports_points_behind_the_near_plane},
      {"track", straight_segments_run_along_z},
      {"track", curves_ease_in_hold_and_ease_out},
      {"track", a_right_hand_quarter_turn_ends_up_facing_right},
      {"track", road_sections_chain_their_heights},
      {"track", straights_and_curves_stay_flat},
      {"track", track_refuses_to_grow_past_capacity},
      {"track", segment_lookup_wraps_around_the_track},
      {"track", points_along_the_track_follow_the_centreline_and_its_sides},
      {"track", slope_along_the_track_is_continuous},
      {"track", rumble_stripes_alternate_every_rumble_length},
      {"track", oval_track_bends_the_same_way_twice},
      {"track", a_closed_track_joins_its_last_segment_to_its_first},
      {"spline", spline_passes_through_its_control_points},
      {"spline", spline_bulges_outwards_between_the_corners_of_a_square},
      {"generator", generated_tracks_are_well_formed_for_many_seeds},
      {"generator", generated_track_never_comes_close_to_itself},
      {"generator", generated_track_depends_only_on_the_seed},
      {"clip", polygons_in_front_of_the_near_plane_are_untouched},
      {"clip", polygons_behind_the_near_plane_vanish},
      {"clip", a_straddling_polygon_is_cut_along_the_near_plane},
      {"car", car_accelerates_from_rest},
      {"car", acceleration_fades_as_speed_rises},
      {"car", top_speed_is_reached_on_the_flat_and_never_exceeded},
      {"car", braking_beats_coasting_and_both_stop_the_car},
      {"car", uphill_slows_a_coasting_car_faster_than_the_flat},
      {"car", flat_out_up_the_steepest_hill_keeps_most_of_the_speed},
      {"car", a_parked_car_rolls_down_a_hill},
      {"car", downhill_exceeds_the_flat_top_speed_but_not_the_limit},
      {"car", car_position_wraps_at_the_end_of_the_track},
      {"car", a_parked_car_does_not_steer},
      {"car", steering_builds_up_and_carries_on_after_release},
      {"car", steering_is_measured_rather_than_twitchy},
      {"car", car_cannot_leave_the_verge},
      {"car", bends_push_harder_at_higher_speed},
      {"car", grass_slows_the_car_and_loosens_its_grip},
      {"car", car_ignores_bad_time_steps},
      {"car", an_airborne_car_neither_drives_nor_steers},
      {"car", car_remembers_its_last_acceleration},
      {"body", body_rides_on_the_road_surface},
      {"body", a_fast_car_flies_over_a_sharp_crest_and_lands},
      {"body", a_slow_car_keeps_its_wheels_on_the_crest},
      {"body", landing_compresses_the_suspension_which_then_settles},
      {"body", grass_shakes_the_body_and_tarmac_does_not},
      {"body", body_squats_under_power_and_dives_under_braking},
      {"body", body_rolls_away_from_the_direction_of_travel},
      {"body", a_jolt_kicks_the_suspension_and_dies_away},
      {"dashboard", needles_rest_at_zero_and_sweep_with_speed_and_revs},
      {"dashboard", steering_wheel_turns_the_way_the_car_steers},
      {"dashboard", dashboard_moves_with_the_body},
      {"dashboard", dial_needles_start_bottom_left_and_sweep_clockwise},
      {"dashboard", gear_lever_sits_in_the_h_pattern_slot_of_the_gear},
      {"dashboard", dashboard_sits_low_and_leaves_the_road_visible},
      {"dashboard", dashboard_layout_moves_with_the_body},
      {"dashboard", speed_reads_in_miles_per_hour},
      {"wheel", steering_wheel_is_see_through_except_rim_spokes_and_hub},
      {"wheel", steering_wheel_edges_are_smooth_and_symmetric},
      {"wheel", steering_wheel_is_lit_from_the_top_left},
      {"lever", gear_lever_starts_in_its_gear_and_stays_put},
      {"lever", gear_lever_slides_straight_down_from_first_to_second},
      {"lever", gear_lever_follows_the_h_through_the_neutral_bar},
      {"gate", gear_gate_plate_is_metal_with_dark_slots},
      {"gate", gear_lever_has_a_knob_on_a_shaft_and_nothing_beside_it},
      {"dial", dial_face_has_a_chrome_bezel_ticks_and_a_red_zone},
      {"dial", needle_is_a_red_blade_with_a_hub_and_nothing_beside_it},
      {"view", cockpit_camera_sits_above_the_car_facing_down_the_road},
      {"view", cockpit_camera_pitches_with_the_slope_and_the_body},
      {"view", straight_road_stays_centred_and_narrows},
      {"view", polygons_are_drawn_far_to_near},
      {"view", right_hand_bend_shifts_the_distance_to_the_right},
      {"view", nothing_behind_the_camera_or_off_screen_is_kept},
      {"view", car_offset_moves_the_road_the_other_way},
      {"view", road_polygons_carry_texture_rows},
      {"view", near_road_is_split_finer_than_far_road},
      {"view", camera_rises_with_a_raised_road},
      {"view", road_climbing_ahead_rises_on_screen_and_so_does_the_horizon},
      {"view", billboards_stand_on_the_ground_and_sort_with_the_road},
      {"view", a_closed_circuit_is_drawn_only_within_the_draw_distance},
      {"view", rear_view_looks_back_down_the_road},
      {"view", mirror_shows_what_is_behind_on_the_same_side},
      {"scenery", scenery_stands_beside_the_road_on_both_sides},
      {"scenery", scenery_depends_only_on_the_seed},
      {"scenery", scenery_blocks_a_car_that_drives_into_it},
      {"traffic", traffic_is_spread_along_the_lanes},
      {"traffic", traffic_drives_on_and_wraps_around_the_lap},
      {"traffic", traffic_keeps_its_distance_from_the_car_ahead},
      {"traffic", running_into_traffic_slows_the_player_to_its_speed},
      {"traffic", traffic_billboards_sit_on_the_road_in_their_lane},
      {"traffic", traffic_includes_slower_longer_trucks},
      {"traffic", trucks_are_drawn_bigger_and_stop_the_car_further_back},
      {"texture", asphalt_is_grey_noise_around_the_base_colour},
      {"texture", asphalt_is_deterministic_for_a_seed},
      {"note", engine_revs_climb_through_each_gear_and_drop_at_the_change},
      {"note", gear_indicator_climbs_from_first_to_top},
      {"note", firing_rate_follows_the_revs_continuously},
      {"note", engine_fires_at_the_firing_rate_with_jitter},
      {"note", engine_is_louder_and_harsher_under_throttle},
      {"note", engine_output_stays_in_range_and_obeys_the_volume},
      {"note", revs_change_smoothly_rather_than_in_steps},
      {"note", skid_loop_is_noise_with_a_soft_join},
      {"note", crash_thump_starts_loud_and_dies_away},
      {"note", engine_is_louder_under_throttle_and_skid_needs_speed},
      {"laps", simulation_barely_depends_on_the_step_size},
      {"laps", lap_timer_counts_laps_and_keeps_the_best},
      {"laps", rolling_backwards_over_the_line_does_not_count},
      {"laps", lap_time_formats_as_minutes_seconds_and_tenths},
      {"stage", stage_simulates_identically_at_any_render_rate},
      {"stage", stage_caps_catch_up_after_a_stall},
      {"stage", stage_uses_the_curve_under_the_car},
      {"stage", stage_uses_the_slope_under_the_car},
      {"stage", stage_track_comes_from_the_seed},
      {"stage", stage_stops_the_car_behind_traffic},
      {"stage", stage_shows_traffic_behind_in_the_mirror},
      {"stage", stage_crashes_the_car_into_a_tree},
      {"stage", stage_counts_laps},
      {"stage", stage_releases_everything},
  };
  if (argc < 2) {
    fprintf(stderr, "usage: tests <group>\n");
    return 2;
  }
  int ran = 0;
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (strcmp(cases[i].name, argv[1]) == 0) {
      cases[i].run();
      ++ran;
    }
  }
  if (ran == 0) {
    fprintf(stderr, "unknown group %s\n", argv[1]);
    return 2;
  }
  return 0;
}
