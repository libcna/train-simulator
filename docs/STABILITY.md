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
a saved screenshot. The hours-long resource run is still pending.
