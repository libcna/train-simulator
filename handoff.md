# Train Simulator — AI development handoff

Updated: 2026-10-11 (input repairs verified; long stability run pending).
Repository: `libcna/train-simulator`, current branch `develop`. Written for the next agent
(expected: Codex, "astra high") starting from a fresh context.

## Owner instructions so far

- 2026-10-09: finish the river-bank repair, write this English handoff, commit,
  push, stop. Done; that commit was the initial project snapshot.
- 2026-10-10: resume from this document and continue the visual campaign until
  every ranked finding is done, pushing after each group. Done: ENV-067 to
  ENV-123 were repaired, committed and pushed one group at a time (see the Git
  history). The owner then asked to finish the current item, rewrite this
  handoff, commit, push and stop. **Do not continue work on your own; wait for
  the owner's next instruction.** The owner manages the application goal; never
  create or change goals implicitly.
- 2026-10-11: the owner resumed from this handoff, then explicitly requested
  implementation of the agreed work followed by an indefinite loop of fresh
  Train Simulator audits, proposed improvements and implementation. The earlier
  stop instruction is superseded. Continue useful work without asking between
  groups. The owner still manages the application goal; do not mark this ongoing
  goal complete merely because one engineering item is finished.
- Later on 2026-10-11 the owner narrowed the goal to the work estimated at
  roughly 4–6 hours: finish streaming and its validation; perform a long
  keyboard/mouse, six-car/interactions check and fix reproduced issues; run a
  multi-hour stability/memory test, evaluate it and fix discovered failures.
  **Do not start further audits or add unrelated improvements after those items.**
  This supersedes the indefinite audit loop requested earlier the same day.
- The owner's conversation is Czech; all repository content, commit messages
  and this document are English.

## What this project is

A playable but unfinished C++23 first-person passenger simulator on CNA's
OpenGL 3.3 renderer. The fictional Northern Valley Express runs a 286.4 km,
12-station route indefinitely, reversing at both termini. Six passenger cars,
seating, gangways, toilet interactions, passenger/conductor CNA avatars, route
map, passenger displays, generated railway audio and offline English
announcements are integrated. The player stays aboard; it is not a driving game.
Interactive launches open on a start menu (Start / Enter / Space; F12 quits);
automated runs with `--frames`, `--run-seconds`, `--screenshot` or
`--interaction-smoke` skip it.

Read, in this order: `README.md`, `docs/ARCHITECTURE.md`, `docs/VISUAL_AUDIT.md`,
`docs/VISUAL_CAMPAIGN.md`, `docs/AUDIT.md`, `bugs.md`, then `docs/STREAMING.md`. Automated tests do not
prove realism; only inspected engine screenshots do.

## Non-negotiable constraints

- Sibling dependencies `../cna` (CNA `next`) and `../sharp-runtime` are
  immutable: never edit, patch, stash or commit in them. Record probable
  dependency defects in `bugs.md`; implement workarounds here. Other agents work
  in those checkouts concurrently; on 2026-10-10 `sharp-runtime` briefly failed
  to compile mid-edit and recovered on its own. If that happens, wait and retry;
  do not touch it.
- Desktop CNA **OPENGL33 only**. No other renderers, web/mobile or platform
  work. Keep simulation renderer-independent.
- C++23, MIT project licence; keep third-party notices in `docs/ASSETS.md` and
  `assets/licenses/`.
- Build with at most **four compilation workers**, the system `/usr/bin/ccache`,
  `CCACHE_DIR=/rv/cnaccache`, `CCACHE_BASEDIR=/rv`. Never create another cache
  or change the shared cache size. Use only the existing `build/`; no clean
  rebuilds, duplicate dependency clones or new build directories.
- Never run two heavy jobs at once (build, CTest, GPU captures). Other agents
  share the host; load averages above 20 were common on 2026-10-10 and distort
  frame-time measurements.
- Preserve unrelated work. Do not spawn subagents unless the owner says so.

## Build, run and verification

```bash
export CCACHE_DIR=/rv/cnaccache CCACHE_BASEDIR=/rv
cmake --build build --parallel 4                 # incremental; already configured
./build/trainsim_render_tests                    # 178 render/geometry unit tests
./build/trainsim_tests                           # 128 simulation unit tests
env LP_NUM_THREADS=4 ctest --test-dir build --parallel 1 --output-on-failure   # 69 tests; shared-host duration varies
./build/train-simulator                          # interactive play (start menu)
```

Private hardware-GPU captures (Radeon 780M via a private Weston/Xwayland; never
touches the owner's desktop):

```bash
env LP_NUM_THREADS=4 ../cna/tools/platform/run_gpu_tests_private.sh --exec \
  env SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy bash tools/<capture script>.sh after
```

CTest offscreen cases use software Mesa. Private-compositor frame pacing of
about 26 ms is **not** a 60 FPS claim.

## Audit workflow and evidence (how every repair was done)

`docs/VISUAL_AUDIT.md` is the authoritative ranked list and repair ledger: 123
findings (ENV-001 to ENV-123), **all addressed with evidence** as of this
handoff. Each evidence section names a capture prefix, the inspected matched
before/after frames, the tests and a 600-frame stationary performance sample.
Do not reopen closed IDs; a new visual pass starts from new screenshots and a
new ranked list.

The repeatable method, to reuse for any future repair:

1. Write or reuse a `tools/capture_<topic>.sh` script (pattern: `before|after`
   stage, optional prefix, fixed `--audit-camera` poses, a 600-frame
   `--report-state` performance run, a binary SHA-256 receipt; refuses to
   overwrite a preserved `before`).
2. Capture `before` with the current binary and inspect the real images.
3. Implement in a focused header under `include/trainsim/render/` and wire it
   into `src/render/SceneRenderer.cpp`; add unit tests in
   `tests/MeshBuilderTests.cpp`; expose a `--report-state` counter when a smoke
   test can use it.
4. Build, run both unit test binaries, capture `after`, inspect, compare
   performance, run the whole CTest suite on the final binary, record
   everything in `docs/VISUAL_AUDIT.md` (ranked row + evidence section),
   `docs/ARCHITECTURE.md` and here, then commit and push.
5. If a new pose is needed after the `before` stage, rebuild the old binary with
   `git stash push -- include src tests` (never stash scripts or docs), capture
   the new pose under a new prefix, pop and rebuild.

Audit camera: train-local metres, absolute render Y (seated eye 1.90 m,
standing 2.36 m); yaw 0 looks toward negative Z, +90 toward positive X;
positive pitch looks up; route distance grows toward local positive Z; the
cars are centred at Z = -52.5 + 21·car (car 0..5); the cab bulkheads are at
Z = ±63.2 and the noses at ±72.8. Station command-line indices are one-based.
`--audit-camera` requires `--frames` and `--screenshot` (a performance run must
not use it). Static captures disable avatars/audio and nearly freeze route time.

All QA images and logs are local, ignored files under `build/visual-audit/`;
keep them. A fresh clone can reproduce `after` captures but not the historical
`before` binaries.

## Implementation map of the 2026-10-10 session (ENV-067 to ENV-123)

New focused headers, each with unit tests, each wired into `SceneRenderer.cpp`:

| Header | Covers | Report counters / scripts |
| --- | --- | --- |
| `BridgeGeometry.hpp` | bridge parapets (upstand, coping, cable trough, tubular handrail on 2.5 m posts), viaduct valley profile (`viaductValleyTerrain`, bowl + spurs, seamless abutments), footed/tied piers | `bridge_parapet_posts`; `capture_bridge_detail.sh` (prefixes `bridge-parapet`, `viaduct-valley`) |
| `TunnelGeometry.hpp` | precast lining (textured face in the concrete batch, ring/longitudinal joints, haunch), bracketed luminaires with seamless wash, handrail/cable tray, escape-route and hectometre signs (`Sim::kTunnelExtentsMetres`) | `tunnel_lining_rings`, `tunnel_marker_plates` (required by the tunnel smoke test); `capture_tunnel_detail.sh` (`tunnel-lining`, `tunnel-markers`) |
| `TrackGeometry.hpp` + `makeBallastDetailImage` | angular ballast texture, sloped shoulders to a wobbling toe, chamfered monoblock sleepers inside the fastener radius, foot/web/head rail profile | `capture_track_detail.sh` (`track-surface`) |
| `LinesideGeometry.hpp`, `SignalGeometry.hpp` | `addBarBetween` chord bars (catenary wires now follow curves), wire fences with strained ends, rebuilt colour-light signal (lens centres shared with the live aspect overlay) | `fence_posts`; `capture_lineside_detail.sh` (`lineside`) |
| `InteriorFittingsGeometry.hpp` + `addSeatRearShell` | seat grip recess, literature pocket, pedestals, rack suitcases, cab-door hardware, extinguisher, intercom, seated fasteners, gangway plate, grab-rail caps, ceiling diffusers | `capture_interior_fittings.sh` (`interior-fittings`, `interior-close`), `capture_hvac_detail.sh` (`hvac-grille`, `hvac-close`) |
| `ToiletGeometry.hpp` | toilet, support rail, paper dispenser, coves, vanity + sensor spout, gradient mirror, dispensers, threshold | `capture_toilet_detail.sh` (`toilet-fittings`; its `mirror` pose is invalid, see the audit) |
| `TrainExteriorGeometry.hpp` | lower bodyside with bogie cut-outs, bogies, underframe modules, livery bands, window gaskets, mullion faces, door reveals, roof crown + HVAC + pantographs (cars 2 and 5), inter-car bellows, cab nose/lamps/coupler/visor/gasket/wipers | `capture_train_exterior.sh` (`train-exterior`) |
| `TerminalGeometry.hpp` | buffer stops, train shed, head house with name lettering, forecourt taxi rank; the track batch now overruns both route ends by 125 m (`kTerminalTrackOverrunMetres`) | `terminal_buffer_stops`, `terminal_shed_columns`; `capture_terminal_composition.sh` (`terminal-composition`) |

Also: `Route.hpp` exports `kValleyViaductBegin/EndMetres` and
`kTunnelExtentsMetres`. `SceneRenderer.cpp` remains large (~9,300 lines); keep
adding reusable geometry in headers rather than inline.

## Known open issues (outside the ranked list)

1. **Streaming follow-up.** CPU-only `WorldGeometry` preparation now runs on one
   worker, with a route copy and a separate deterministic world snapshot. The
   render thread stages at most one non-empty material per frame into a second
   GPU window; meshes, counters and anchor swap together at the boundary. Empty
   slots are skipped immediately (new software-rendering smokes caught wasted
   frames here). Startup, teleport, settings changes and late predictions use a
   complete-scene synchronous fallback. See `docs/STREAMING.md` (PERF-001) for
   evidence, telemetry semantics and reproduction. Final moving
   evidence reduced p99 from 173.4 to 26.9 ms with 45 ready swaps and no late
   predictions; the avatar-enabled ride measured p99 27.1 ms with 18 ready
   visuals and the same 45 ready swaps. Final verification is recorded below.
   Remaining risks are large
   individual uploads, separate track rebuilds, heavy-load missed deadlines and
   the extra GPU window's memory footprint; do not claim every hitch is gone.
2. **Intermittent physical-mouse camera freeze** reported by the owner; not
   reproduced by the 59-minute, 180-cycle private native X11 input test. Six-car
   walking and seating/display/toilet checks exposed and repaired crowd-yield
   deadlocks and an NPC reservation race during player toilet entry. See
   `docs/STABILITY.md` for exact component results and harness limits. Injected
   events cannot certify the owner's physical mouse; do not claim its issue fixed.
3. **No hours-long stability/leak run** exists. Do not claim one.
4. Several frame-time comparisons on 2026-10-10 were distorted by host load
   from other agents (documented per section). Re-measure on a quiet host before
   treating any p95/p99 shift as a regression.
5. Remaining visual gaps are design-level (stylised avatars, repetitive
   procedural buildings, flat vertex-colour materials in places); they are not
   listed defects and need a new screenshot-based ranking if the owner wants
   another pass.

## Current continuation verification

The original 2026-10-10 checkpoint was commit `41cbc1d`, 67/67 CTests,
176 render tests and 124 simulation tests. The resumed checkout was clean on
`develop`, pointing to the same commit as `main`; the continuation stays on
`develop` rather than moving the owner's branch.

The streaming continuation adds `WorldGeometry.hpp`, preparation telemetry,
full vertex/index/counter equivalence tests, two directional prefetch smokes,
`tools/capture_streaming.sh` and `tools/test_world_prefetch_input.sh`. Its exact
results are in `docs/STREAMING.md` and `build/visual-audit/streaming-*`.

Final streaming build: **69/69 CTests passed, 339.49 s**; render/geometry
**178/178**, simulation **124/124**. The private settings/teleport input probe
passed with 12 synchronous loads and no late preparation. Matched static captures,
1,800-frame no-avatar and 1,800-frame populated GPU rides were inspected. Peak RSS
was about 844 MiB without avatars and 996 MiB with avatars in those short runs;
these are not long-term leak results. CNA `2b15506a4` and sharp-runtime `c20373ba`
were clean and untouched by this project.

The streaming group is committed and pushed as `9ccf390` to `origin/develop`;
the owner explicitly approved that destination. Input work now has 128/128 passing
simulation tests and 178/178 render tests. The final full CTest run passed 68/69;
the gallery validator timed out at 15 s, then passed alone in 4.17 s. Both receipts
are retained; do not describe that as a single clean full-suite pass.
The 20-second stability-helper smoke passed; the 2.5-hour resource journey and
its evaluation remain required before the finite goal is complete. Current tools:
`tools/test_native_walk.py` (zero rounds isolates interactions),
`tools/run_stability_soak.py` and the extended `tools/test_x11_input.sh`.
Do not start a new visual audit or other additional work. Dependencies remain
read-only. Verify commits and worktree state with `git log -1` / `git status -sb`;
do not rely on this document alone.
