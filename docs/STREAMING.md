# Prepared world streaming

## PERF-001 — periodic full-window rebuild stalls

The 2026-10-11 continuation starts after the closed ENV-001–123 visual campaign.
A fresh moving High-quality GPU baseline reproduced the open synchronous streaming
problem: steady p99 **173.4 ms**, maximum **204.0 ms**, and a maximum world rebuild
of **201.1 ms**. The scene rebuilt all seven chunks on each 250 m boundary.

`WorldGeometry` now generates CPU-only material meshes and their counters in a
single worker. Each job owns a route copy and a deterministic world snapshot;
it cannot access the graphics device, avatars, or mutable simulation state.
The renderer prepares the next chunk in the service direction immediately after
activating the current one. When generation completes, it uploads at most one non-empty material
per frame (empty slots are retired immediately) into a second GPU window. Crossing the boundary swaps all materials,
anchor and counters together. The previous window becomes staging storage for
the next preparation. No historical windows accumulate.

Initial loading, teleports, changed quality/vegetation range and missed deadlines
retain a synchronous fallback so the player never sees geometry from the wrong
location or quality. Reversal discards a now-irrelevant prediction. Shutdown joins
the worker, whose inputs and CPU output are independently owned. CPU builders
retain capacity; GPU buffers retain the existing exact-size Discard policy.

The render thread owns every GPU creation, upload, swap and destruction. This is
one CPU window and two GPU windows, not a worker graphics context or unbounded
cache. Preparing a window may still cost over 100 ms on the worker. An individual
large material upload and the separately rebuilt track can still cause a long
frame; the fallback can still hitch during unusually fast travel or heavy load.

## Evidence

All evidence is local under `build/visual-audit/`. Reproduce with
`tools/capture_streaming.sh before|after [prefix]` inside the private GPU wrapper
from `handoff.md`. The script preserves a historical before receipt, captures
seven static views plus a 600-frame stationary sample, and runs 1,800 moving
frames at 8x journey time from km 52, High, car 2 window, without avatars/audio.
Use the exact same executable for each stage's static and moving captures.

| Moving sample, 1,619 steady frames | Before | After |
| --- | ---: | ---: |
| Mean frame interval | 28.1 ms | 25.5 ms |
| p95 | 26.0 ms | 26.1 ms |
| p99 | 173.4 ms | 26.9 ms |
| Maximum steady interval | 204.0 ms | 37.1 ms |
| Activated windows | 51 | 46 |
| Prepared boundary swaps | unavailable | 45 |
| Synchronous builds | 51 | 1 (initial load) |
| Late preparations | unavailable | 0 |

Logs/images: `streaming-moving-{before,after}.{log,png}`. Both final images were
inspected as actual engine output. The fixed-frame runs end at different route
positions (km 64.619 / 63.419): reduced stalls change elapsed simulation-update
time. This is a matched starting scenario, not identical per-chunk timing.
The private compositor's ~26 ms cadence remains unrelated to a 60 FPS claim.
The incremental build also picked up an upstream CNA `Game.cpp` default-depth
initialization update since the historical executable; this is not a dependency-
pinned benchmark. Final checked dependencies are CNA `2b15506a4` and sharp-runtime
`c20373ba`, both clean and untouched by this project. The preparation counters
independently establish that full-window work no longer occurs at ordinary
boundaries in the tested run.
After-run peak RSS was **864,192 KiB** (about 844 MiB); the old run did not record
RSS, so no memory improvement is claimed. The extra GPU window has a memory cost.

A separate final 1,800-frame moving High passenger-view run kept avatars enabled:
all 18 visuals were ready, 12 NPCs visible, and all 45 boundary swaps were prepared
with no late prediction. Its 1,619 steady samples measured mean 25.5 ms, p95 26.2 ms,
p99 27.1 ms and maximum 36.6 ms. Peak RSS was 1,019,824 KiB (about 996 MiB).
The populated interior image was inspected (`streaming-avatars-after.{log,png}`).
This checks worker/avatar coexistence in a short ride, not hours of stability.

The 600-frame stationary controls have 419 steady samples each: before/after
mean 25.8/25.6 ms, p95 26.7/26.6 ms, p99 27.3/27.1 ms, maximum 28.0/27.7 ms.

Static evidence is `streaming-static-*-{before,after}.png`, with binary receipts
and stationary logs using the same prefix. The seated-window pair preserves
terrain, planting, fencing and distant composition. Across all seven pairs,
changed pixels range from zero to 0.01671%, with mean absolute channel difference
at most 0.000246/255; shoulder, signal and track-down are pixel-identical. These nearly frozen
captures still have small time-dependent/raster differences. They do not prove
motion continuity alone.

The CPU regression compares every vertex position, normal, UV, colour, index and
all geometry counters between foreground retained chunks and an independently
owned asynchronous snapshot. It reuses both outputs through countryside,
adjacent chunks, Medium station surroundings, High tunnel, Low viaduct and both
terminals. Endpoint selection tests cover forward/reverse and short routes.
The first runtime prefetch smokes exposed a wasted-frame bug on slow software
rendering: assigning a full frame to each empty Low material made every
prediction late. Empty slots now retire immediately around the single non-empty
upload; the regression retains its requirement for a prepared boundary hit.
Runtime prefetch smokes exercise both directions; the existing terminal-reversal
smoke covers service direction changes. `tools/test_world_prefetch_input.sh` uses
private X11 input to change quality High → Medium → Low → Medium → High, change
vegetation range in both directions, and repeatedly teleport between stations.
It passed with 12 synchronous loads, two prepared swaps, no late preparation,
seven loaded chunks and final High quality. Both its settings and station
captures were inspected (`streaming-invalidation-*.png`).

Final incremental build and the complete **69/69 CTest suite passed in 339.49 s**
(`streaming-full-tests.log`). The suite includes **178/178 render/geometry** and
**124/124 simulation** cases, the directional preparation regressions, avatar
lifecycle, station exchanges, terminal reversal and install-layout validation.
The initial suite's only two failures were the new directional regressions;
`streaming-full-tests-initial.log` preserves that evidence before the empty-slot
fix. No dependency source was changed. Shell syntax and patch whitespace checks
also passed.

## Diagnostic semantics

- `world_prefetch_hits`: boundary activations with all materials uploaded already.
- `world_synchronous_builds`: direct CPU builds (including initial loading).
- `world_late_prefetches`: requested predictions that still need generation or
  uploads at their boundary; fallback completes them before publishing.
- `world_rebuilds`: activated complete windows, preserving its earlier meaning.
- `world_rebuild_*_ms`: render-thread streaming work over all frames, including
  staged uploads and fallback waits, excluding background generation.
- `world_generation_*_ms`: completed CPU geometry generation, now usually on the
  worker; a prepared next window may be counted before it becomes visible.
- `world_upload_total_ms`: accumulated CPU-side uploads; `world_upload_max_ms`
  now measures the largest single material submission, rather than an entire
  window. It is not GPU-completion timing and should not be compared directly
  with the previous whole-window maximum.

The original periodic full-window stall is addressed in this measured corridor.
Keep monitoring other route types, avatars and longer journeys; this is not an
hours-long stability certification or a claim that every possible hitch is gone.
