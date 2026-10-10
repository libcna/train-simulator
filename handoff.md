# Train Simulator — AI development handoff

Updated: 2026-10-10. Repository: `libcna/train-simulator`, branch `main`.

## Owner instruction at handoff

On 2026-10-09 the owner asked to finish the river-bank repair, write this
English handoff, commit, push and stop. On 2026-10-10 the owner asked to resume
from this document and continue the visual work until all findings are done;
ENV-067 through ENV-104 were repaired so far in that session. Treat each session's instruction as its own scope. The owner manages the application goal;
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

- `TunnelGeometry.hpp`: textured precast lining with ring/longitudinal joints,
  haunch, bracketed handrail/cable tray, bracketed wall luminaires with a
  seamless gradient wash, escape-route and hectometre signs per niche bay.
  `Route.hpp` exports `kTunnelExtentsMetres`. `tools/capture_tunnel_detail.sh`
  captures the tunnel (`tunnel-markers` prefix adds the 123.0 km bay views).

- `TrackGeometry.hpp`: ballast toe wobble, chamfered monoblock sleepers inside
  the fastener radius, foot/web/head rail profile; sloped ballast shoulders and
  the reworked angular ballast texture. `tools/capture_track_detail.sh`.

- `LinesideGeometry.hpp` / `SignalGeometry.hpp`: chord bars (catenary wires now
  follow curves), wire fences with strained ends, rebuilt colour-light signal.
  `tools/capture_lineside_detail.sh`.

- `InteriorFittingsGeometry.hpp` plus `addSeatRearShell`: grip recess, literature
  pocket, pedestals, suitcases, cab-door hardware, extinguisher, intercom,
  fasteners, gangway plate and rail caps. `tools/capture_interior_fittings.sh`
  (`interior-close` prefix adds luggage/cab-door/end-wall close views).

Latest evidence prefixes: `terminal-forecourt-`, `water-surface-`, `lake-bank-`,
`river-bank-`, `bridge-parapet-`, `viaduct-valley-`, `tunnel-lining-`,
`tunnel-markers-`, `track-surface-`, `lineside-`, `interior-fittings-`,
`interior-close-` and `toilet-fittings-` (`ToiletGeometry.hpp`,
`tools/capture_toilet_detail.sh`). The terminal's comparable before benchmark is specifically
`terminal-forecourt-terminal-performance-before.log`; the earlier similarly
named baseline measured Stonebridge and is not comparable. The lake's
`far-bank-before` camera lay below the old coarse terrain and was excluded from
acceptance; use the valid near-bank, passenger and oblique pairs.

## Remaining risks and next work after a new owner request

1. Continue the ranked visual audit with the HVAC grilles ENV-121, then the
   exterior train shell/equipment ENV-105 to ENV-119 and the terminal
   composition ENV-120. ENV-121 remains open. ENV-120 is the broader terminal
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

- **106 addressed / 17 open / 123 total findings.** ENV-001 through ENV-104,
  ENV-122 and ENV-123 have evidence; ENV-105 through ENV-120 and ENV-121 remain.
- Final incremental build succeeded with shared system ccache and four workers.
  During the session the sibling `sharp-runtime` checkout was briefly being
  edited by someone else and failed to compile; it recovered on its own and was
  not touched from this project.
- Each repair's whole CTest run on its final binary is logged as
  `build/visual-audit/<prefix>-full-tests.log` (latest: `toilet-fittings-`,
  67/67). Render unit tests: 171/171 after the toilet work.
- Final actual GPU screenshots were inspected for every repair, matched against
  preserved before images; see the audit's evidence sections and prefixes.
- The latest 600-frame tunnel sample: **87 → 89 draws**, **783,628 → 839,908
  triangles**; steady mean **25.4 → 25.4 ms**, p99 **26.8 → 27.2 ms**, maximum
  **32.5 → 32.2 ms**; initial rebuild **17.6 → 22.9 ms**. The viaduct sample's
  initial rebuild is **145.6 ms** (synchronous generation cost, still open). None
  of this replaces a moving or long physical-input test.
- CNA and sharp-runtime working trees remained clean. No dependency modifications.
- The repository history starts with the 2026-10-09 initial snapshot; later
  commits are ordinary diffs. Ignored build products, runtime configs, logs and
  audit captures are excluded from every commit.
- Verify what is actually on `origin/main` with `git log -1` and
  `git status -sb`; do not assume push state from this document.
