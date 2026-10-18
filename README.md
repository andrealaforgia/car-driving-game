# Car Driving

An arcade racer seen from the cockpit of a Formula One car, built with C and
SDL2 on the same [sdl-2d-game-engine](https://github.com/andrealaforgia/sdl-2d-game-engine)
submodule as [Asteroids](https://github.com/andrealaforgia/asteroids). A
random closed circuit every run, with hills, bends in both directions,
trees and signs beside the road, traffic to overtake or run into, a lap
timer, and distant hills under a hazy sky.

## How it is drawn

The track is real geometry. A closed spline around a wobbly ring is sampled
into segments of equal length, each with a position in the world and a
heading. The camera sits above the car with a yaw that follows the road
and a pitch that follows the slope. Every frame each strip of road, verge,
rumble and lane marking within the draw distance is a quad in the world,
moved into camera space, clipped against the near plane, projected, and
sorted far to near for a painter's draw; the road nearest the camera is
split lengthwise so the affine texturing does not warp. Trees, signs and
cars are billboards sorted into the same list. There is no depth buffer,
so cars stay sprites; the 2D engine's textured triangles are enough for a
road ribbon and billboards.

## Setup

```
make install        # submodule + SDL2 libraries
make dev_install    # cpplint and clang-format (optional)
```

## Build and run

```
make
./car-driving
./car-driving --help
```

The command line options are the engine's: `--window-mode=0..3`, `--vsync`,
`--show-fps`, `--fps=N`, `--volume=0..128`, `--display=N`, `--display-mode=N`.

The circuit, its scenery and its traffic are generated from a seed, logged
at startup. To replay one:

```
CAR_DRIVING_SEED=1974 ./car-driving
```

Every circuit bends both ways, has a real hill, closes on itself, never
comes close to itself, and keeps bend radii and slopes within fixed limits;
candidates that break the limits are rolled again.

## Controls

- **Up** accelerate, **Down** brake, **Left / Right** steer
- **S** mute, **F11** toggle fullscreen, **ESC** quit

## What you see and hear

The driver's view: a translucent dashboard low across the screen with a
speedometer, a rev counter with a red zone, and a leather steering wheel
that turns as the car steers. The wheel is painted at start-up into a
transparent sprite (a shaded rim with stitching, three metal spokes and a
padded hub) and rotated by SDL, so no image files are needed. The whole
dashboard bobs on the suspension, dips under braking, lifts under power
and sways in bends, and over a sharp crest at speed the car takes off and
lands with a thump through the springs. The grass is bumpy.

Traffic cruises in the three lanes at a quarter to a half of your top
speed, slows down behind whatever is ahead of it (you included), and
stops you dead if you run into it. Leaving the road at speed into a tree
or a sign is a crash: most of your speed goes, the car bounces back and
the suspension takes a jolt. The HUD shows the speed in mph at the top centre, the gear, the lap number
with the running lap time, and the best lap once you have one.

All sound is synthesised, so there are no audio assets. The engine is
generated live in the mixer's music hook: a train of exhaust pulses whose
firing rate follows the revs continuously, each pulse a burst of noise
through two resonant pipe filters, with a little jitter in timing and
strength so it sounds like a motor rather than a note, over a bed of intake
noise that grows with revs and throttle. A seven speed gearbox turns speed
into revs, so the note climbs through each gear and drops at the change.
Braking at speed gives a tyre skid, the grass rumbles, and a crash thumps.
`make sound` renders eight seconds of the engine to `engine.wav` so it can
be judged without launching the game. The pixel art (trees, signs,
traffic) is authored as character grids in the source; the road texture
and the hills on the horizon are generated noise.

Top speed on the flat is where engine force balances air drag, so
acceleration fades as you get faster. Gravity acts along the slope: climbs
cost speed, descents give it back and can push you past the flat top speed.
Steering builds lateral speed through the tyres' grip rather than moving the
car instantly, and bends push outwards with the square of speed, so hard
bends have to be taken slower. The grass has far less grip and brakes the
car hard.

## Checks

```
make test        # projection, track, generator, view, car, traffic and stage contracts, no window
make sanitize    # the same under AddressSanitizer and UndefinedBehaviourSanitizer
make lint
make snapshot    # renders one frame headlessly to snapshot.bmp
make sound       # renders the engine accelerating through the gears to engine.wav
./render_snapshot out.bmp 12000 0 7   # position, car x, seed [lateral speed] [altitude]
```

The simulation runs at 240 fixed steps per second, independently of the render
frame rate, with catch-up after a stall capped at 250 milliseconds.
