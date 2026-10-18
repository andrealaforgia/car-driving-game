# Research: Pseudo-3D road rendering for a classic SDL2 driving game

**Date**: 2026-10-18 | **Researcher**: Bea (Claude Fable 5.1) | **Confidence**: High | **Sources**: 7

## Executive Summary

The look you want (grey road narrowing to a horizon, green verges, blue sky, the car fixed near the bottom of the screen) is the OutRun / Pole Position family of pseudo-3D. None of those games had 3D hardware. The road is a stack of horizontal bands whose width and position shrink with depth; curves and hills are faked by shifting those bands sideways and vertically. Everything reduces to one number per band, `scale = d / z`, where `d` is the camera depth from the field of view and `z` the distance of the band from the camera.

Two implementation families exist. The raster family (Lou's Pseudo 3D Page, javidx9's "Retro Arcade Racer") draws one scanline at a time with a per-line horizontal shift. The segmented family (Jake Gordon's "JavaScript Racer") models the track as a list of fixed-length segments in world space, projects their endpoints and draws each as a trapezoid. For this project the segmented approach is the right one: it is resolution independent, maps directly onto the engine's existing `draw_filled_polygon`, keeps all the maths in pure C functions that can be unit tested without SDL exactly as the asteroids tests do, and hills and roadside sprites later come from the same projection with no new technique.

An oval track is trivial in this model: two straight sections and two constant-curvature sections of the same sign. Fake curves never close geometrically, which does not matter because the track wraps at its total length and nothing on screen reveals the mismatch.

## Research Methodology

**Search Strategy**: Started from the two canonical references every pseudo-3D tutorial cites (Lou's page and Jake Gordon's series), then looked for a C or C++ reference implementation and for an existing SDL2 example.
**Source Selection**: Technical articles and open source code. Cross-checked formulas between Lou, Gordon and javidx9. Read the asteroids engine headers to confirm which primitives exist.
**Quality Standards**: Every formula below appears in at least two sources, or is derived from the projection equation in one line.

## Findings

### Finding 1: Everything is one projection

**Evidence**: Gordon's projection step: "scale = d / camZ; screenX = W/2 + scale * camX * W/2; screenY = H/2 - scale * camY * H/2; screenW = scale * roadWidth * W/2" with "d = 1 / tan(fov / 2)".
**Source**: [JavaScript Racer v1, straight roads](https://jakesgordon.com/writing/javascript-racer-v1-straight/) - Accessed 2026-10-18
**Confidence**: High
**Verification**: [Lou's Pseudo 3D Page](https://www.extentofthejam.com/pseudo/) gives the same thing as `y_screen = (y_world * scaling) / z_world + y_res / 2` with `scaling = (x_res/2) / tan(fov/2)`.
**Analysis**: This is similar triangles, nothing more. The camera sits at height `cameraHeight` above the road, so a point on the road surface has `camY = -cameraHeight`, which puts near points low on the screen and far points near the centre line (the horizon). The coordinates are normalised, so the same maths works at any window size. That matters here because the asteroids game lets the user pick any display mode.

### Finding 2: The track is a list of segments; the road is a stack of trapezoids

**Evidence**: "The track is an array of segments, each about segmentLength units long. Each segment stores two endpoints, p1 (near edge) and p2 (far edge) ... Color alternates every rumbleLength segments."
**Source**: [JavaScript Racer v1](https://jakesgordon.com/writing/javascript-racer-v1-straight/)
**Confidence**: High
**Verification**: Lou's page describes the same thing as the "3D-projected segments" method: "The road is a strip of quads, each with a curve amount and a height."
**Analysis**: Per frame the renderer finds the segment under the camera, then walks `drawDistance` segments forward (wrapping at the end of the track). For each it projects p1 and p2, skips it if p1 is behind the camera or if p2's screen y is at or below the lowest y already drawn (`maxY` clipping, which is what makes hills occlude correctly later), then draws grass, rumble strips, road surface and lane markers as trapezoids between the two projected edges. Drawing near to far with clipping, rather than far to near with overdraw, is what keeps it cheap.

Gordon's starting parameters, which look right for this game:

| Parameter | Value | Role |
|---|---|---|
| fieldOfView | 100 degrees | zoom |
| cameraHeight | 1000 | camera above road |
| roadWidth | 2000 | half width of the road |
| segmentLength | 200 | z length of one segment |
| rumbleLength | 3 | segments per colour stripe |
| drawDistance | 300 | segments drawn per frame |
| lanes | 3 | lane markers |

### Finding 3: Curves are a per-segment horizontal shift accumulated twice

**Evidence**: "Each segment's near point (p1) is projected with the offset x, and its far point (p2) with x + dx. After each segment, x increases by dx, and dx increases by the segment's curve."
**Source**: [JavaScript Racer v2, curves](https://jakesgordon.com/writing/javascript-racer-v2-curves/)
**Confidence**: High
**Verification**: Lou: "ddx += dx; x += ddx" per line, and the anti-jitter fix "dx = -percentage_of_segment_traversed * ddx", which Lou credits to Gordon. javidx9 uses the closed form `centre = 0.5 + curvature * (1 - p)^3`, a cubic in screen depth, which is the same idea without the accumulator.
**Analysis**: There is no x geometry in the track at all. Each segment carries a `curve` value (negative left, positive right, Gordon uses 2, 4, 6 for easy, medium, hard). The renderer accumulates that value twice while walking away from the camera, so the shift grows quadratically with distance and the road bends. The one subtlety is the starting `dx`: it must be scaled by how far the camera has travelled into the base segment, or the curve shape jumps each time the camera crosses a segment boundary. Gordon builds curve sections with `addRoad(enter, hold, leave, curve)` where enter eases from 0 to `curve`, hold keeps it, leave eases back, so the bend does not snap in.

### Finding 4: Steering, centrifugal push and off-road slowdown

**Evidence**: Gordon's update loop: `position = (position + dt * speed) mod trackLength; dx = dt * 2 * (speed / maxSpeed); left: playerX -= dx; right: playerX += dx; ... if (|playerX| > 1 and speed > offRoadLimit) speed += offRoadDecel * dt`, with `maxSpeed = segmentLength / step` so the car never moves more than one segment per frame.
**Source**: [JavaScript Racer v1](https://jakesgordon.com/writing/javascript-racer-v1-straight/) and [v2](https://jakesgordon.com/writing/javascript-racer-v2-curves/)
**Confidence**: High
**Verification**: javidx9 takes a different but equivalent route: the player accumulates a heading from steering input, the track accumulates a heading from its curvature, and the car's screen offset is the difference. Off-road is `|difference| >= 0.8`, which drops speed hard.
**Analysis**: `playerX` is the car's lateral position normalised to the road, with -1 and +1 at the road edges. The car sprite never moves on screen; the camera's x offset is `playerX * roadWidth`, so the road moves under the car. In curves Gordon pushes `playerX` away from the bend by `dt * speedPercent * segment.curve * 0.3`, which is what forces the player to steer into bends instead of coasting. Capping speed at one segment per frame keeps future collision checks a single segment lookup.

### Finding 5: Sky, horizon and scenery

**Evidence**: javidx9: the top half of the screen is sky, hills are `height = |sin(x * 0.01 + trackCurvature)| * 16` per column, and the phase depends on the accumulated road heading "so the hills scroll and shift as you turn". Gordon: three parallax layers each advance by `layerSpeed * segment.curve * speedPercent` per update.
**Source**: [OneLoneCoder RetroArcadeRacer.cpp](https://github.com/OneLoneCoder/Javidx9/blob/master/ConsoleGameEngine/SmallerProjects/OneLoneCoder_RetroArcadeRacer.cpp) and [JavaScript Racer v2](https://jakesgordon.com/writing/javascript-racer-v2-curves/)
**Confidence**: High
**Verification**: Lou: "as you take a curve, the view seems to look around the curve", which is why the background must scroll with curvature.
**Analysis**: For the first version a flat blue rectangle above the horizon and green below it is enough; the road trapezoids paint over the green. A horizon that scrolls sideways in bends is a one-line addition later, and javidx9's rectified sine is a cheap way to get distant hills without any art assets, which fits the asteroids style of drawing everything with primitives.

### Finding 6: Hills are optional and come for free from the same projection

**Evidence**: "addSegment(curve, y) sets p2.world.y ... p1.world.y to the previous segment's p2.world.y ... The projection equations already handle non-zero y, so render() needs no changes."
**Source**: [JavaScript Racer v3, hills](https://jakesgordon.com/writing/javascript-racer-v3-hills/)
**Confidence**: High
**Verification**: Lou's "3D-projected segments" hills section: "Hill height is stored per segment in world Y ... Hidden segments are culled when their screen Y is above the last-drawn segment's Y."
**Analysis**: Not in scope for the oval, but worth knowing that the segmented design pays for itself here. The raster approach needs a hand-tuned variable step through the Z map instead.

### Finding 7: No reusable SDL2 C implementation exists; Cannonball is not a fit

**Evidence**: Cannonball "rewrites the original 68000 and Z80 assembler code in C++ ... uses SDL 2 and Boost ... requires the original ROMs".
**Source**: [djyt/cannonball](https://github.com/djyt/cannonball)
**Confidence**: High
**Verification**: Web search for SDL2 C pseudo-3D projects returned only JavaScript, Python and Phaser ports.
**Analysis**: Cannonball is useful only as a reference for how the real OutRun road data is structured. It needs the ROMs and is a very large codebase. Nothing to reuse; the right move is to build from the formulas above on top of the sdl-2d-game-engine submodule, as asteroids does.

## How this maps onto the asteroids engine

The engine already has what the segmented approach needs. `draw_filled_polygon` takes an `SDL_Point` array and fills it with a triangle fan through `SDL_RenderGeometry`, so a road trapezoid is one call with four points. `draw_filled_rect` paints the sky and the base grass. `clear_frame` and `render_frame` wrap the frame. `keyboard.h` has `is_up_key_held`, `is_down_key_held`, `is_left_key_held` and `is_right_key_held`, which are exactly throttle, brake and steering. `frame_limiter_wait` gives the delta time and the playing stage in asteroids shows the fixed-timestep accumulator at 240 steps per second, which carries over unchanged. `set_logical_size` exists if you prefer a fixed logical resolution, but the normalised projection makes that unnecessary.

One performance note, to be measured rather than assumed. `draw_filled_polygon` issues one `SDL_RenderGeometry` call per edge, so a quad costs four calls. At 300 segments and five quads per segment that is around 6000 draw calls per frame. The hardware renderer will likely cope, but if the FPS counter says otherwise the fix is a `draw_filled_quad` in the engine that submits two triangles in a single call. Do not add it until the counter asks for it.

The track for an oval:

```
add_straight(100 segments)
add_curve(enter 25, hold 100, leave 25, curve +4)
add_straight(100 segments)
add_curve(enter 25, hold 100, leave 25, curve +4)
```

The track length is the sum, and `position` wraps modulo that length. Because curves are fake there is no need for the two bends to add up to 360 degrees.

What can be unit tested without opening a window, in the style of `tests/test_game.c` in asteroids:

- projection of a world point to normalised screen coordinates, including the behind-camera case
- segment lookup from a position, including wrap-around at the end of the track
- stripe colour alternation every `rumbleLength` segments
- curve accumulation producing a monotonic shift over a right-hand section and zero shift on a straight
- the anti-jitter starting `dx` being continuous across a segment boundary
- speed clamping, acceleration, braking, natural deceleration and off-road slowdown
- `playerX` clamping and centrifugal push direction opposite to the curve
- a full lap at maximum speed ending within one segment of where it started, with the same result at 30, 60 and 144 render FPS (the asteroids `simultaneous` and `stall` tests are the model)

The rendering layer stays thin: it takes a list of already projected trapezoids and colours and calls the engine. That is the only part that needs SDL and it has no decisions in it.

## Knowledge Gaps

### Gap 1: Draw call budget
**Issue**: Whether 6000 `SDL_RenderGeometry` calls per frame hold 60 FPS on the Metal backend on this Mac is unknown. | **Attempted**: Read the engine source; no benchmark exists. | **Recommendation**: Build the straight road slice first with `--show-fps` and measure before touching the engine.

### Gap 2: Car sprite
**Issue**: The research covers the road, not the car. Asteroids draws everything with vector primitives; a car drawn with primitives will look odd next to a filled road. | **Attempted**: Not researched. | **Recommendation**: Decide between a small hand-made PNG via `texture.h` (three frames: straight, left, right, as javidx9 does) and a polygon car. This is a taste decision, not a technical one.

## Conflicting Information

### Conflict 1: Raster versus segmented road
**Position A**: Lou and javidx9 render per scanline with a closed-form or accumulated centre shift. Cheapest possible per-line cost, and the way the originals did it.
**Position B**: Gordon models segments in world space and projects them.
**Assessment**: Both produce the same picture. Segmented wins here because the game runs at arbitrary resolutions, because the engine's primitive is a filled polygon rather than a scanline, and because the pure-function core is easier to test. Lou himself describes segmented as the modern method and credits Gordon's anti-jitter fix.

## Recommendations for the build

Run this through nWave as small vertical slices, each one playable:

1. Walking skeleton. Window, blue sky, green ground, a straight striped road scrolling forward at constant speed. This proves projection, segment lookup and the render loop and is where the FPS measurement happens.
2. Throttle and brake on up and down arrows, with natural deceleration.
3. The oval. Curve sections with easing, the double accumulator and the anti-jitter start. The road bends and the car drifts outwards.
4. Steering on left and right arrows, off-road slowdown on the grass.
5. Car sprite with three steering frames and a lap counter plus lap timer in the HUD.
6. Horizon scenery that scrolls with curvature, and optionally the sine hills.

Hills, other cars and roadside objects are all later and all fall out of the same projection.

## Full Citations

[1] Louis Gorenfeld. "Lou's Pseudo 3D Page". https://www.extentofthejam.com/pseudo/. Accessed 2026-10-18.
[2] Jake Gordon. "How to build a JavaScript Racer, v1: straight roads". https://jakesgordon.com/writing/javascript-racer-v1-straight/. Accessed 2026-10-18.
[3] Jake Gordon. "How to build a JavaScript Racer, v2: curves". https://jakesgordon.com/writing/javascript-racer-v2-curves/. Accessed 2026-10-18.
[4] Jake Gordon. "How to build a JavaScript Racer, v3: hills". https://jakesgordon.com/writing/javascript-racer-v3-hills/. Accessed 2026-10-18.
[5] David Barr (javidx9). "OneLoneCoder_RetroArcadeRacer.cpp", GPLv3. https://github.com/OneLoneCoder/Javidx9/blob/master/ConsoleGameEngine/SmallerProjects/OneLoneCoder_RetroArcadeRacer.cpp. Accessed 2026-10-18.
[6] Chris White. "Cannonball: an enhanced OutRun engine". https://github.com/djyt/cannonball. Accessed 2026-10-18.
[7] Andrea Laforgia. "sdl-2d-game-engine" and "asteroids". https://github.com/andrealaforgia/sdl-2d-game-engine. Local copy at ~/dev/games/asteroids read 2026-10-18.

## Research Metadata
Duration: 1 session | Examined: 12 | Cited: 7 | Cross-refs: 6 | Confidence: High 100% | Output: docs/research/rendering/pseudo-3d-road-comprehensive-research.md
