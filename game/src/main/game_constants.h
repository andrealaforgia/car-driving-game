/**
 * @file game_constants.h
 * @brief Tuning values for the road, the camera and the car.
 */

#ifndef GAME_SRC_MAIN_GAME_CONSTANTS_H_
#define GAME_SRC_MAIN_GAME_CONSTANTS_H_

#define PHYSICS_BASELINE_FPS 60.0
#define SIMULATION_STEPS_PER_SECOND 240.0
#define MAX_FRAME_CATCH_UP_MS 250.0

/* Road geometry, in world units. */
#define SEGMENT_LENGTH 200.0
#define ROAD_HALF_WIDTH 2000.0
#define RUMBLE_LENGTH 3
#define LANE_COUNT 3
#define DRAW_DISTANCE 300
#define MAX_TRACK_SEGMENTS 4000

/* Camera. Nothing closer than the near plane is drawn. */
#define CAMERA_FIELD_OF_VIEW_DEGREES 100.0
#define CAMERA_HEIGHT 650.0
#define CAMERA_NEAR_PLANE 50.0
#define MIRROR_FIELD_OF_VIEW_DEGREES 70.0

/* Car, in world units per second. CAR_SPEED_LIMIT keeps the car under one
 * segment per frame at the physics baseline; CAR_MAX_SPEED is the top speed
 * on the flat, where engine force and drag balance. */
#define CAR_SPEED_LIMIT (SEGMENT_LENGTH * PHYSICS_BASELINE_FPS)
#define CAR_MAX_SPEED (CAR_SPEED_LIMIT * 0.8)
#define CAR_ENGINE_ACCELERATION (CAR_MAX_SPEED / 3.0)
#define CAR_DRAG_COEFFICIENT \
  (CAR_ENGINE_ACCELERATION / (CAR_MAX_SPEED * CAR_MAX_SPEED))
/* Rolling resistance: a small constant (static friction) plus a term
 * proportional to speed. */
#define CAR_STATIC_FRICTION (-CAR_MAX_SPEED / 80.0)
#define CAR_ROLLING_RESISTANCE (-CAR_MAX_SPEED / 10.0)
#define CAR_BRAKING_DECELERATION (-CAR_MAX_SPEED / 1.5)
/* Engine braking on the overrun, at the red line. */
#define CAR_ENGINE_BRAKING (CAR_MAX_SPEED / 12.0)
/* The hardest the car can accelerate or brake, for weight transfer. */
#define CAR_LONGITUDINAL_LIMIT (CAR_ENGINE_ACCELERATION * 2.5)
/* Reverse gear: gentle, and capped well below forward speeds. */
#define CAR_REVERSE_SPEED (CAR_MAX_SPEED * 0.15)
#define CAR_REVERSE_ACCELERATION (CAR_ENGINE_ACCELERATION * 0.5)
/* Gravity: the pull along the road's slope is light enough that flat out
 * up the steepest hill keeps most of the top speed, while a parked car
 * still rolls down; the body falls faster than that when airborne. */
#define CAR_GRAVITY (CAR_MAX_SPEED / 6.0)
#define CAR_FALL_GRAVITY (CAR_MAX_SPEED / 2.0)
/* What the top speed on the flat reads as on the speedometer. */
#define CAR_TOP_SPEED_MPH 150.0
#define CAR_OFF_ROAD_DECELERATION (-CAR_MAX_SPEED / 2.0)
#define CAR_OFF_ROAD_LIMIT (CAR_MAX_SPEED / 4.0)
#define CAR_MAX_X 2.0
/* Half the car's width in road half widths, for collisions. */
#define CAR_HALF_WIDTH 0.2
/* Hitting something solid: the speed that is left, and the jolt. */
#define CRASH_SPEED_FACTOR 0.2
#define CRASH_JOLT 1.0
/* Lateral speed in road widths per second at full lock, before the
 * steering goes heavy with speed, and how quickly the car reaches it
 * (per second). */
#define CAR_STEERING_RATE 1.8
#define CAR_STEERING_WEIGHT 0.6
#define CAR_GRIP_ON_ROAD 5.0
#define CAR_GRIP_ON_GRASS 2.0
/* The most the tyres can change the lateral speed in a second, and how
 * much of that is lost at full braking or acceleration. */
#define CAR_LATERAL_GRIP 3.2
#define CAR_LATERAL_GRIP_ON_GRASS 1.6
#define CAR_WEIGHT_TRANSFER 0.7
#define CAR_CENTRIFUGAL 0.35

/* Car body: flight over crests, suspension, pitch and roll. Suspension
 * travel is normalised (1 = fully compressed). */
#define BODY_GRAVITY CAR_FALL_GRAVITY
#define BODY_MIN_HOP_ALTITUDE 100.0
#define SUSPENSION_STIFFNESS 400.0
#define SUSPENSION_DAMPING 25.0
#define SUSPENSION_MAX_TRAVEL 1.0
#define LANDING_KICK 20.0
#define GRASS_BUMP_LENGTH 1600.0
#define GRASS_SHAKE_FORCE 100.0
#define PITCH_RESPONSE 10.0
#define ROLL_RESPONSE 8.0

/* Scenery stands this far from the road centre, in road half widths. */
#define SCENERY_NEAREST_LATERAL 1.3
#define SCENERY_FARTHEST_LATERAL 3.5
#define SCENERY_HIT_RANGE 120.0

/* Traffic: lane positions in road half widths, sizes in world units,
 * speeds in world units per second. */
#define TRAFFIC_COUNT 16
#define TRAFFIC_CAR_LENGTH 450.0
#define TRAFFIC_CAR_WIDTH 0.45
#define TRAFFIC_MIN_SPEED (CAR_MAX_SPEED * 0.25)
#define TRAFFIC_MAX_SPEED (CAR_MAX_SPEED * 0.45)
#define TRAFFIC_FOLLOW_DISTANCE (3.0 * TRAFFIC_CAR_LENGTH)
#define TRAFFIC_BRAKE_RATE 10.0
#define TRAFFIC_ACCELERATION_RATE 1.0
/* Every fourth vehicle is a truck: longer, wider and slower. */
#define TRAFFIC_TRUCK_EVERY 4
#define TRAFFIC_TRUCK_LENGTH 900.0
#define TRAFFIC_TRUCK_WIDTH 0.62
#define TRAFFIC_TRUCK_MIN_SPEED (0.18 * CAR_MAX_SPEED)
#define TRAFFIC_TRUCK_MAX_SPEED (0.28 * CAR_MAX_SPEED)
#define TRAFFIC_SHOVE 1.0

#endif  // GAME_SRC_MAIN_GAME_CONSTANTS_H_
