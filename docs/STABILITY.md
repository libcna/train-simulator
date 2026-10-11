# Long-running input and stability validation

This is the finite follow-up to [streaming preparation](STREAMING.md), not a new
visual audit. The scope is extended native mouse/keyboard input, six-car walking
and interactions, and an hours-long populated journey with process-resource
sampling. Findings require actual reproduction before changing gameplay.

## Reproduction

Use the existing OpenGL 3.3 executable in `build/`. Run only one GPU test, build
or CTest job at a time. These tools use the sibling CNA runner read-only and
refuse input outside its private Weston/Xwayland display. They require
`xdotool` and ImageMagick `import` for input/captures and Python 3 for the two
journey helpers. They never send input to the owner's desktop.

```bash
INPUT_PROBE_BACKEND=xwayland INPUT_PROBE_QUALITY=2 INPUT_PROBE_AVATARS=1 \
  INPUT_PROBE_PHASES=180 INPUT_PROBE_OUTPUT=build/visual-audit/input-long \
  LP_NUM_THREADS=4 bash tools/test_x11_input.sh

python3 tools/test_native_walk.py --rounds 4 --prefix native-walk

python3 tools/run_stability_soak.py --seconds 9000 --time-scale 2 \
  --interval 10 --prefix stability-long
```

Python helpers refuse to overwrite an existing evidence prefix. All images,
logs and receipts remain ignored under `build/visual-audit/`. The input shell
probe defaults to `build/x11-input-test`; choose a distinct output directory to
retain a previous run. Its initial Enter key starts the interactive menu.

The input probe checks actual yaw travel on every motion phase and alternates
pause, map and external-window focus loss before reacquisition. It ends with a
stand-and-walk check. It uses High quality and avatars above; audio is disabled
in this specific probe. Injected native X11 events exercise event delivery and
camera updates, but cannot certify the owner's physical mouse or compositor.

The walking helper uses ordinary mouse look and W/A/S/D/E interactions. Use
`--rounds 0` to reproduce only seating/display/toilet interactions; this mode
does not claim six-car traversal coverage. It
stands and sits repeatedly, traverses all six cars and their gangways, presses
against both cab boundaries, opens a passenger display, enters the toilet,
closes/reopens its door from inside and exits. Collision and passenger blocking
remain active. It records screenshots and checks trace positions and final
interaction counters. Audio and High-budget avatars are enabled.

The soak starts at the first station with avatars and audio enabled (SDL dummy
audio output). Every ten seconds it samples the owned process's RSS, peak RSS,
virtual memory, threads and file descriptors from `/proc`, alongside the latest
five-second application trace. It records executable SHA-256, arguments and
Git revision. Final checks require clean exit, expected retained populations and
a saved screenshot; a run of at least two hours at scale two also requires a
terminal reversal and subsequent travel. The deadline is bounded and cleanup
targets only the process group owned by the helper.

## Interpretation

Inspect the time series, including repeated route sections, warm-up and late
plateaus. A single peak RSS measurement cannot establish whether memory leaks.
Stable RSS, thread/file-descriptor counts and bounded application populations
over one finite journey provide evidence only for that scenario. Audio output
is exercised through the dummy driver; this is not a listening test.

The frame p99 in periodic traces is a rolling 2,048-frame window. A missed
five-second trace interval can reveal a stalled update loop; the trace does not
record typed text. Do not call an unreproduced physical input problem fixed.

## Results

### Extended mouse/keyboard event delivery — passed

The 2026-10-11 private GPU run completed **180/180 phases in 59 min 7.03 s**,
High quality with all 18 avatar visuals ready. Evidence:
`build/visual-audit/input-long/`, `input-long-runner.log`; a three-phase setup
check is retained separately in `input-smoke/` and `input-smoke-runner.log`.

- All motion phases advanced actual camera yaw. Final counters reported 181
  capture acquisitions and 60 expected focus losses. The final stand/walk check
  and application exit passed.
- Across 707 periodic traces, the longest interval was 5.07385 s. The journey
  visited both directions, including a terminal reversal, and ended with ten
  completed station legs.
- Streaming completed 1,092 prepared swaps, one initial synchronous build and
  zero late predictions. Seven world chunks, 38 passengers and 18 ready visuals
  remained at completion.
- `/usr/bin/time -v` reported 1,251,812 KiB peak RSS for the command tree. This
  does not replace a process-specific memory time series.
- Captures at phases 50 and 179 were inspected. They show a rendered populated
  carriage during the outward and return journeys. This was an input/stability
  check, not a fresh cosmetic audit.

No camera freeze was reproduced by these injected native events. The reported
physical-mouse issue remains unconfirmed; this result does not establish a fix.
The stale probe startup was corrected to press Enter on the interactive start
menu before attempting mouse capture.

### Six-car walk — reproduced obstruction repaired

The initial controller made undersized diagonal sidesteps; a second version
overshot the aisle edge and encountered seat collision. Those harness failures
are retained in `native-walk/` and `native-walk-lanes/`. Feedback-controlled
sidesteps and backing away then reproduced a persistent crowded-aisle blockage
(`native-walk-feedback/`, `native-walk-diagnostic/`).

Additional `nearby_actors` trace data (identity, X, Z, numeric activity) showed
actors standing abreast in opposite lanes near Z=10.5 m. Their sideways yield
could not cross the other actor's body clearance. The player could be boxed
between this pair and another pair behind. The same yielding logic could also
move an actor into the toilet's side wall.

Two focused simulation regressions failed before the repair. Yielding now checks
the interior footprint, chooses the usable side next to the toilet wall, and
allows a blocked actor to retreat gradually away from the player before trying
sideways again. A centred player gives the group one stable preferred side;
actors notice the player within 2.5 m and release the manoeuvre beyond 3 m, so
the back of a nearby queue can make room. If the wall forces an actor onto the
player's side, it retreats along the wall instead of blocking that side forever.
Retreat retains actor/player separation, train bounds and current car identity.

The regression now drives a collision-limited player through groups of two and
six actors, starting centred and on either side, and separately checks the
toilet wall footprint. The repair passed all 126 then-existing simulation tests.

`native-crowd-fixed/` subsequently completed both six-car traversals and the
information display interaction. Its later direct centre-aisle walk to the toilet
was blocked by an actor; the controller now keeps an offset lane for that move.
An earlier `native-walk-final/` run completed seven traversals before a too-loose
sidestep tolerance stopped the eighth at the body-clearance boundary. These are
component results, not complete passing script runs.

### Toilet entry priority — repaired and validated

`native-toilet-final/` reached the door from the opposite wall, allowing a
reserved NPC visit to finish. It opened the door but was then blocked at
X=0.642, Z=4.537 m: another passenger could claim the room while the player was
still entering. The open-door flag alone did not protect the approach.

Opening the door now gives the player priority while within the same 1.8 m
entrance reach used for interaction. Being inside also retains priority. Closing
the door outside or walking away releases it; an existing NPC reservation still
prevents the player opening the door. Two new tests cover the doorway/inside/
departure states and actual NPC reservation attempts during entry and after
departure. **128/128 simulation tests passed.**

The final `native-toilet-priority/` run passed in **206.89 s**, with three sit and
four stand transitions, the display opened/closed, four player toilet-door
operations, entry, a closed-door wait inside, reopening, exit and closing from
outside. It ended outside with the door closed and exited normally. This
`--rounds 0` run complements the completed six-car component checks above; it
does not count as another full-train walk. The display capture from
`native-crowd-fixed/` and both toilet captures from `native-toilet-priority/`
were inspected.

### Final checks before the long resource run

The full final CTest invocation passed 68/69 tests in 146.91 s. The unchanged
gallery-PNG validator exceeded its 15 s timeout; its isolated rerun passed in
4.17 s. Both outcomes are preserved in `interaction-final-ctest.log` and
`interaction-showcase-recheck.log`. Core coverage is **128/128 simulation** and
**178/178 render/geometry** tests. Shell/Python syntax checks and `git diff
--check` passed. No dependency checkout was modified.

The helper smoke (`stability-smoke-*`) completed normally in 23.614 wall seconds
for a requested 20-second journey, yielding twelve process-resource samples and
a saved screenshot. The completed hours-long resource run is recorded below.

The initial `stability-long-*` attempt was interrupted by a host restart after
3,000 seconds of sampled wall time. On resuming, its tool handle was gone, the
host had only about four minutes of uptime, and a host-level process check
confirmed that neither the game nor its Python runner survived. Its last trace
was at 2,997.89 application seconds, with 814 prepared swaps and zero late
predictions. There is no final state, exit receipt or screenshot; this is an
incomplete run, not a passing long test or evidence of an application crash.
The original files are preserved. A fresh uninterrupted 9,000-second attempt
uses the distinct `stability-restart-*` prefix.

### Uninterrupted populated resource journey — passed

The replacement run completed **9,000 requested application seconds in
9,001.817 wall seconds (2 h 30 min 1.817 s)**, with normal process exit and all
helper invariants passing. It used High quality, 18 avatar visuals and enabled
audio through SDL's dummy output. Evidence is under `build/visual-audit/`:
`stability-restart-{manifest,summary,analysis}.json`,
`stability-restart-samples.jsonl`, `stability-restart-game.log`,
`stability-restart-runner.log` and `stability-restart-final.png`.

The executable SHA-256 remained
`071285233baafd7dbeada5f34c5e01d057323c15074172afad55a7c819bc4457`,
recorded against commit `fdb2941`. No build or other heavy test ran alongside it.
The final screenshot was inspected: the populated carriage, landscape and HUD
remained rendered. Both dependency checkouts were still clean at CNA
`2b15506a4` and sharp-runtime `c20373ba`.

- The train completed 24 station legs, reversed at both termini and began a
  third traversal, finishing at km 45.052 in the original direction. This covers
  approximately 617.9 km of simulated travel, including repeated route sections.
- All **2,470 boundary swaps** used prepared scenery; there was one initial
  synchronous build and **zero late preparations** throughout the trace.
- There were 901 process-resource samples and 1,796 application traces. The
  longest periodic trace interval was 5.04263 s. Seven chunks and 38 passengers
  remained bounded throughout. After the initial frame-zero trace, every trace
  had all 18 avatar visuals ready, including the final state.
- Steady sampled thread counts stayed at 21–22 (the preparation worker is
  temporary), and file descriptors stayed at 10. Playing audio instances stayed
  at 12–14, with zero paused instances.
- The kernel RSS high-water mark was **1,210.3 MiB**; the largest ten-second RSS
  sample was 1,185.1 MiB. The last sample was **940.2 MiB**. The final quarter's
  median was **933.8 MiB**, with a range of 844.0–1,147.5 MiB.

| Wall-time interval | Median process RSS |
| --- | ---: |
| 0–30 min | 952.7 MiB |
| 30–60 min | 947.9 MiB |
| 60–90 min | 963.0 MiB |
| 90–120 min | 941.1 MiB |
| 120–150 min | 933.0 MiB |

The final traversal repeated km 0–40 in the original direction. Median RSS in
the 10–20, 20–30 and 30–40 km bins decreased from 954.5/976.1/970.5 MiB on the
first visit to 936.9/932.2/951.9 MiB on the repeat. The startup-containing 0–10 km
bin was higher on the repeat (890.4 → 971.8 MiB); the later bins and time blocks
did not continue that increase. This finite scenario shows bounded resource use
and no sustained RSS growth. It does not prove that every allocation is freed,
measure GPU memory separately or establish leak freedom for other scenarios.

Final rolling frame p99 was 28.5 ms over 2,048 samples. The whole-run maximum
frame interval was 309.0 ms, including startup: zero late scenery preparations
is not a claim that every frame hitch is eliminated. Audio used dummy output,
and this unattended journey does not resolve the unconfirmed physical-mouse
report. No new application failure requiring a repair was found in this run.

The agreed streaming, extended input/interactions and multi-hour resource checks
are complete. No further audit campaign is part of this scope.
