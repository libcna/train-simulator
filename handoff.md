# Train Simulator — AI development handoff

Updated: 2026-10-10. Repository: `libcna/train-simulator`, branch `main`.

## Owner instruction at handoff

On 2026-10-09 the owner asked to finish the river-bank repair, write this
English handoff, commit, push and stop. On 2026-10-10 the owner asked to resume
from this document and continue the visual work; ENV-067 and ENV-068 were
repaired in that session. Treat each session's instruction as its own scope. The owner manages the application goal;
do not block implementation on the goal service or create/change goals implicitly.

## What this project is

A playable but unfinished C++23 first-person passenger simulator using CNA's
OpenGL 3.3 renderer. The fictional Northern Valley Express traverses a 286.4 km,
12-station route indefinitely, reversing at terminals. Six passenger cars,
seating, gangways, toilet interactions, passenger/conductor CNA avatars, route
map, passenger displays, generated railway audio and offline English announcements
are integrated. This is not a driving game; the player stays aboard.

Read `README.md`, `docs/ARCHITECTURE.md`, `docs/VISUAL_AUDIT.md`,
`docs/VISUAL_CAMPAIGN.md`, `docs/AUDIT.md`, and `bugs.md` before continuing.
The visual campaign is not finished, and automated tests do not prove realism.

## Non-negotiable constraints

- Repository content and commit messages: English. The owner's conversation is Czech.
- CNA `next` at `97783d1e0`; sharp-runtime `next` at `21c06910` were the tested
  sibling dependencies. Never edit, patch or commit either dependency. Record
  probable dependency defects in `bugs.md`; implement workarounds in this project.
- Develop/test desktop CNA **OPENGL33 only**. No alternative renderers, web/mobile,
  or platform-expansion work. Driver logs may report GL 4.6 capability; CNA still
  selects OPENGL33. Keep simulation renderer-independent.
- C++23, MIT project license; preserve third-party font/asset notices in
  `docs/ASSETS.md` and `assets/licenses/`.
- Maximum **four compilation workers**. Mandatory system `/usr/bin/ccache`, shared
  `CCACHE_DIR=/rv/cnaccache`, `CCACHE_BASEDIR=/rv`. Never create another cache or
  change the shared cache size. Retry with elevated permissions if sandbox access
  to the existing cache is rejected; do not silently disable ccache.
- Use only the existing `build/`. No clean rebuild, duplicate dependency clone,
  alternate ticket build tree or multi-gigabyte asset download without necessity.
- Preserve unrelated work. Use `apply_patch` for source/document edits. Do not
  spawn subagents unless the owner or applicable instructions explicitly authorize them.

## Build, run and verification

Workspace used: `/rv/data/development/github.com/libcna/train-simulator`.
Read applicable ancestor `AGENTS.md`; the owner's explicit four-worker limit
overrides its generic unrestricted-parallelism advice.

```bash
env CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv cmake --build build --parallel 4
env CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv ccache --show-stats
env LP_NUM_THREADS=4 ctest --test-dir build --parallel 1 --output-on-failure
./build/train-simulator
```

The existing CMake C/C++ launcher is
`/usr/bin/cmake;-E;env;CCACHE_BASEDIR=/rv;/usr/bin/ccache`.
Use README configuration instructions only if genuinely needed; incremental
builds already work. `-Wpedantic` warnings about dependency `__int128` are not a
reason to modify sharp-runtime.

For private hardware-GPU captures, without sending input to the owner's desktop:

```bash
env LP_NUM_THREADS=4 ../cna/tools/platform/run_gpu_tests_private.sh --exec \
  env SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
  bash tools/capture_water_detail.sh after river-bank
```

Run only one heavy job at a time. Use background execution/polling for long jobs
and keep progress messages concise; do not block communication with long waits.
The private Weston/Xwayland run uses Radeon 780M hardware. CTest offscreen cases
use software Mesa. Private-compositor pacing around 26 ms is **not a 60 FPS claim**.

## Audit workflow and evidence

`docs/VISUAL_AUDIT.md` is the authoritative ranked checklist and repair ledger.
Its original 120 IDs are stable. Three screenshot discoveries extended scope to
123: ENV-121 ceiling HVAC grilles, ENV-122 canopy/display intersection, ENV-123
far-lake streamed-bank termination. Do not confuse `docs/AUDIT.md` (older general
audit) with this screenshot-based list.

Capture real engine images before implementation and after the final build;
actually inspect them. Do not generate or paint substitute evidence. Scripts
write binary SHA-256 receipts and refuse to overwrite a preserved `before`
baseline. Never edit a shell capture script while it is running: that previously
caused an EOF failure. Source edits during an old-binary capture are possible,
but do not rebuild/replace that binary until the capture finishes.

QA screenshots and logs live under ignored `build/visual-audit/` and are local,
not included in the Git push. Preserve them on this machine. A fresh clone can
reproduce final captures using committed scripts but cannot reconstruct historical
before binaries from this initial commit. The five `docs/screenshots/` images
are earlier showcase captures, not current audit acceptance evidence.

Relevant capture scripts: `capture_station_detail.sh`, `capture_water_detail.sh`,
`capture_urban_detail.sh`, the interior/window/seat-specific scripts, and
`capture_visual_audit.sh` (original baseline; do not overwrite it).

Audit camera uses train-local metres and **absolute render Y**, not an offset
above the floor. Seated eye is 1.90 m; standing eye is 2.36 m. Yaw 0 looks toward
negative Z, +90 toward positive X; positive pitch looks up. Route distance grows
toward local positive Z. Station command-line indices are one-based. Static
audit captures deliberately disable avatars/audio and nearly freeze route time;
they do not establish movement, physical mouse input or audio correctness.

## Recent implementation map

- `StationGeometry.hpp` / `PlatformLayout.hpp`: canopy structural bays, lamps,
  staggered paving, tactile relief, bins, fence endpoints, station glazing,
  material families, clock turret and finished departure-board case. Display
  position and text clear the canopy. The turret clock was previously inside
  the solid tower; it now has an exposed face and service-time hands.
- `StationSite.hpp` / `ForecourtGeometry.hpp`: both terminal forecourts, graded
  paving, six planted beds and four benches, with deterministic feature
  reservation. Non-overlapping grout strips fixed real depth artifacts.
- `OpenGL33WaterEffect.*` / `WaterSurface.hpp`: one-pass Medium/High water with
  filtered analytic ripple normals, clear-sky Fresnel and bounded time phases.
  Low or shader-invalid fallback uses the original material. This is not actual
  scene reflection/refraction. `water_shader_draws` exposes the chosen path.
  Detached bright lake/river streak boxes were removed at all qualities.
- `ShoreGeometry.hpp`: water-relative terrain contour clipping, rooted-bank
  coordinate search and bounded shoreline irregularity. Terrain triangles are
  split rather than overlaid, preserving area/planes/UVs/winding.
  The completed river-bank repair adds independent bank phases and stable reed/
  shrub groups in Medium/High; `river_bank_clumps` is regression-tested.
- `LandscapeVariation.hpp::streamedBackdropRange`: 500 m background-only
  terrain/water extensions beyond outer detail chunks, retaining seven simulation
  chunks. This addresses ENV-123; it does not fix synchronous rebuild hitches.
- `SceneRenderer.cpp` remains large. Keep new reusable geometry in focused headers
  rather than expanding unrelated systems or moving rendering into simulation.

- `BridgeGeometry.hpp`: shared bridge parapet (upstand, coping, drip line, cable
  trough, tubular two-rail handrail on 2.5 m posts; Low omits base plates) and
  the viaduct valley profile (`viaductValleyTerrain`: bowl deepest at mid-span,
  side spurs, zero weight at both abutments). `Route.hpp` exports the viaduct
  extent constants. Viaduct piers thicken with height, gain footings and ties.
  `bridge_parapet_posts` is reported; `tools/capture_bridge_detail.sh` captures
  both crossings (extra valley/abutment views under the `viaduct-valley` prefix).

Latest evidence prefixes: `terminal-forecourt-`, `water-surface-`, `lake-bank-`,
`river-bank-`, `bridge-parapet-` and `viaduct-valley-`. The terminal's comparable before benchmark is specifically
`terminal-forecourt-terminal-performance-before.log`; the earlier similarly
named baseline measured Stonebridge and is not comparable. The lake's
`far-bank-before` camera lay below the old coarse terrain and was excluded from
acceptance; use the valid near-bank, passenger and oblique pairs.

## Remaining risks and next work after a new owner request

1. Continue the ranked visual audit with tunnel lining/lamps/markers ENV-069 to
   ENV-071, track/ballast/signals/catenary ENV-072 to ENV-080, close interior
   fittings and exterior train shell/equipment. ENV-121 remains open. ENV-120 is the broader terminal
   composition, not closed merely by the forecourt repair.
2. The owner reported an intermittent physical-mouse camera freeze. It remains
   unresolved/not reproduced by the latest visual work. Automated interaction
   tests do not close it. Conduct a genuinely long keyboard/mouse play test,
   traversing all six cars, seating/standing, toilet and doors when authorized.
3. Synchronous streaming hitches remain open. Earlier 1,800-frame, 8x-time,
   avatar-enabled rural run had p99 106.2 ms, maximum 124.7 ms, largest rebuild
   114.5 ms and peak RSS 719,100 KiB. See `docs/VISUAL_CAMPAIGN.md`. Frozen-route
   averages hide this problem. Added detail has increased some initial rebuild
   times; measure moving runs and pursue staged/cached generation without CNA edits.
4. No hours-long leak/stability proof or physical-input play test was completed
   in this latest continuation. Do not claim otherwise. Keep periodic moving,
   avatar-enabled and endpoint-reversal tests alongside visual captures.

Do not infer that the entire campaign is done from a clean Git tree.

## Final verification and disposition

- **70 addressed / 53 open / 123 total findings.** ENV-001 through ENV-068,
  ENV-122 and ENV-123 have evidence; ENV-069 through ENV-120 and ENV-121 remain.
- Final incremental build succeeded with shared system ccache and four workers.
  During the session the sibling `sharp-runtime` checkout was briefly being
  edited by someone else and failed to compile; it recovered on its own and was
  not touched from this project.
- Final whole CTest suite on the final binary: **67/67 passed, 105.79 s**,
  `build/visual-audit/bridge-parapet-full-tests.log` (an earlier 67/67, 131.26 s
  run preceded the final post-spacing change; render unit tests 156/156 after it).
- Final actual GPU screenshots inspected: river/viaduct window, downward,
  oblique, valley floor, along-track, underside and both abutments, matched
  against preserved before images. Prefixes `bridge-parapet-`, `viaduct-valley-`.
- 600-frame viaduct sample: **95 draws unchanged**, **1,223,684 → 1,259,312
  triangles**, **1,240 parapet posts**. Steady mean **25.4 → 25.4 ms**, p95
  **26.0 → 25.7 ms**, p99 **26.2 → 26.0 ms**, maximum **26.3 → 26.9 ms**. Initial
  rebuild **127.4 → 145.6 ms** (synchronous generation cost, still open). This
  does not replace a moving or long physical-input test.
- CNA and sharp-runtime working trees remained clean. No dependency modifications.
- The repository history starts with the 2026-10-09 initial snapshot; later
  commits are ordinary diffs. Ignored build products, runtime configs, logs and
  audit captures are excluded from every commit.
- Verify what is actually on `origin/main` with `git log -1` and
  `git status -sb`; do not assume push state from this document.
