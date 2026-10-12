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
The subsequent populated 2.5-hour journey in [STABILITY.md](STABILITY.md)
completed 2,470 prepared swaps and both terminal reversals with zero late
preparations. Neither result claims that every possible frame hitch is gone.

## PERF-002 — remaining streaming risks

PERF-001 left four named risks: large individual uploads, separately rebuilt track, missed
deadlines under heavy load, and the memory of the second GPU window. The 2026-10-12 pass
measured each with new diagnostics, found a fifth (a hard buffer-size limit) and repaired the
ones that caused hitches or failures.

### Diagnostics added

- `world_gpu_mb`, `world_largest_material`, `world_largest_material_mb`,
  `world_largest_material_vertices`, `world_gpu_segments`: GPU payload of the active window.
- `track_rebuilds`, `track_rebuild_{total,max}_ms` (render-thread time, including the initial
  synchronous build), `track_generation_max_ms`, `track_prefetch_hits`,
  `track_synchronous_builds`, `track_late_prefetches`.
- [`tools/test_streaming_risks.py`](../tools/test_streaming_risks.py): six fixed moving
  scenarios on the private GPU display that also sample the process's own amdgpu memory from
  `/proc/<pid>/fdinfo` (`drm-memory-vram`, `drm-memory-gtt`).
- `trainsim_world_benchmark` (developer target, excluded from normal builds): thread-CPU time
  of complete world windows.

### Findings

1. **Buffer limit (new, fatal).** CNA rejects any vertex buffer above the graphics profile's
   67,108,863 bytes. On the first pass-2 build the opaque world material at Alderwick Central
   held 1.69 M vertices (60.8 MB of vertex data), and at Northbridge
   (km 192.4) it exceeded the limit: the simulator stopped with *"The vertex buffer exceeds the
   active graphics profile limit"* (`audit2-repairs-station-9-after.log` of the first attempt).
   Subtracting the counted pass-2 additions (about 330 k vertices of palisade, trough and
   balconies) puts the pre-pass-2 material near 49 MB, so the margin was already thin.
2. **Large individual uploads.** A whole material was submitted in one frame; the before
   ride's largest single submission was 41.5 ms.
3. **Track rebuilds.** The 2 km track batch was regenerated and uploaded synchronously on the
   render thread at every 500 m anchor: 24 rebuilds in the 8x ride, 40 ms average and 59.1 ms
   maximum (`streaming-risk-probe-x8-km52.log`).
4. **Missed deadlines.** With one material per frame, staging needed up to fourteen frames,
   so preparation throughput, not generation, limited fast travel.
5. **Second GPU window.** One world window is 61–76 MB of vertex and index data (90 MB in the
   densest city window); a track batch is about 18 MB.

### Repairs

- `meshSegments` splits any mesh into contiguous spans of at most 262,144 vertices (9 MiB);
  `GpuMesh` holds one buffer pair per segment and draws them in turn. Small meshes keep one
  segment and their former upload path. Segment spans are prepared on the worker.
- `GpuMesh::stageSegment` writes one segment in 3 MiB slices through CNA's offset uploads.
  Staging continues within a 4 ms per-frame budget, so small materials share a frame, and
  `stagedUploadBytesPerFrame` raises the per-frame amount when the remaining distance to the
  boundary (from the recent metres per frame) would otherwise be too short.
- `TrackGeometry` moves track generation into a CPU-only builder. A worker prepares the next
  500 m anchor in the service direction; the render thread stages it like the world window and
  swaps all three track meshes at the switch point. Teleports, quality changes and reversals
  keep the synchronous fallback.
- Discarded predictions reset partial staging (`GpuMesh::resetStaging`).

### Results

Final binary `3e89842194b2…f0e`, private Radeon 780M, High, window view, no avatars, 1,800
frames (3,600 at 1x), load average 2–3 during the runs. Logs, per-run GPU samples and the
summary are `build/visual-audit/streaming-risk-final-*`.

| Scenario | p95 / p99 / max (ms) | world hits / late | track hits / late | largest slice | GPU VRAM + GTT | peak RSS |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 8x from km 52 (repeat) | 26.0 / 26.5 / 29.5 | 45 / 0 | 23 / 0 | 18.7 ms | 43 + 395 MiB | 833 MiB |
| 16x from km 52 | 26.3 / 26.8 / 33.9 | 109 / 0 | 55 / 0 | 19.2 ms | 40 + 415 MiB | 855 MiB |
| 24x from km 52 | 26.7 / 39.4 / 56.7 | 150 / 3 | 76 / 1 | 23.8 ms | 50 + 460 MiB | 970 MiB |
| 8x through Northbridge | 26.1 / 26.6 / 27.2 | 41 / 0 | 21 / 0 | 20.0 ms | 67 + 412 MiB | 972 MiB |
| 1x through Northbridge | 26.3 / 26.7 / 29.0 | 15 / 0 | 8 / 0 | 13.5 ms | 80 + 379 MiB | 903 MiB |
| 8x on two CPU cores | 26.2 / 26.6 / 27.8 | 45 / 0 | 23 / 0 | 24.7 ms | 70 + 377 MiB | 823 MiB |

The first 8x run of the batch, immediately after the private compositor started, measured
p99 37.4 ms and maximum 105.1 ms with a 384 ms generation outlier; an immediate repeat and the
two-core run on the same route were clean, so this is recorded as an unattributed transient,
not hidden. The matched visual-audit ride (`audit2-repairs-moving-*`) improved from p99 27.3 /
maximum 55.9 ms to 26.8 / 27.8 ms, and its largest single upload from 41.5 to 17.0 ms. The
Northbridge station capture that previously aborted now renders its 1.86 M opaque vertices in
eight segments (20 world segments in total, 136 draw calls).

Thread-CPU generation of a complete High window (`trainsim_world_benchmark 2 50 56 0.25`, host
load about 8) averaged 212.6 ms (maximum 283.1 ms); Medium 119.7 ms; Low 17.2 ms; the
Northbridge approach 188.7 ms with a 75.8 MB largest material. A track batch takes 17–19 ms on
the worker (42–87 ms with two cores or under load).

### Remaining limits

- At 24x journey speed generation, not staging, is the limit: 3 world and 1 track preparations
  were late, each completed synchronously.
- Initial loads, teleports and setting changes still build and upload a whole window and track
  batch in one frame (hundreds of milliseconds), as designed.
- `world_upload_max_ms` and the "largest slice" column time one slice submission, not GPU
  completion. Driver allocation of a new buffer size can still cost tens of milliseconds.
- The GPU figures are this process's own amdgpu client memory; on this APU most buffers live in
  GTT. They are not a portable VRAM budget and include textures, avatars and the train.
- Segmentation adds one draw call per extra segment (none to three in the measured windows).
- No result here claims that every frame hitch is gone; the private compositor's ~26 ms cadence
  also hides any finer pacing behaviour.

## PERF-003 — parallel world preparation

`WorldGeometry::build` now generates the window's chunks on up to
`WorldGeometry::defaultWorkerThreads()` = clamp(hardware threads / 2, 1, 4) threads, each into its
own `WorldBuildContext` (material builders, counters, route-point cache), then appends the parts
in chunk order with `MeshBuilder::append` and `WorldGeometryCounters::operator+=`. Stations are
built afterwards on the calling thread. The unit test "parallel chunk generation reproduces the
sequential world window exactly" compares 1 and 4 threads vertex for vertex at four route
positions and qualities. The streaming worker itself still runs one preparation at a time.

`tools/benchmark_world.cpp` (target `trainsim_world_benchmark`, excluded from the default build)
times 17 windows spread over the route:

| Run | Host load | 1 thread | 2 threads | 4 threads |
| --- | --- | --- | --- | --- |
| Earlier session, before ENV-137-139 | about 4.5 | 121.8 ms | 79.1 ms | 55.6 ms |
| This session, with night glow (`2 0 286.4 17 N`) | about 26 | 320.9 ms (max 520.8) | — | 150.1 ms (max 340.3) |

The second row was measured while other agents loaded the shared host and is not a regression
figure; it shows the same roughly 2-2.5x parallel gain. Re-measure on a quiet host before comparing
absolute times.

### Additions with ENV-137 to ENV-139

- Oncoming services come from `StreamingWorld::otherTrains()`, a window of eight chunks each side
  computed from chunk indices alone. The scenery window and its generation cost are unchanged.
- Each build receives the shadow-casting sun (`SunPosition`), captured when the job starts and
  refreshed by the renderer only in whole-degree steps. It is not part of the job's validity key,
  so a prepared window is never discarded because the clock moved; at worst its baked projected
  shadows are one degree behind the sun.
- `WorldMaterial::NightGlow` is a fifteenth world material (lit windows, lamp heads and pools). It
  is always built (a few thousand triangles in towns) and drawn only after dusk.
- The 8x moving rides in `docs/VISUAL_AUDIT_2.md` (default, night, rain, changing) all kept 45
  prepared swaps with no late preparation.
