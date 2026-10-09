# Dependency issues

## First BasicEffect textured draw differs from an identical immediate repeat (probable)

- **Dependency:** CNA OPENGL33; exact first-use state cause is not yet isolated to dependency
  internals. The project-owned calibration fixture reproduces it without changing CNA.
- **Affected revision:** CNA `next` `97783d1e0`, sharp-runtime `next` `21c06910`.
- **Symptoms:** The first textured BasicEffect quad appears untextured; a second identical
  application/draw samples the bound masonry texture correctly.
- **Reproduction:** Build `trainsim_colour_probe` with `cmake --build build --target
  trainsim_colour_probe --parallel 4`. Run `env SDL_VIDEODRIVER=offscreen
  SDL_AUDIODRIVER=dummy LP_NUM_THREADS=4 ./build/trainsim_colour_probe
  build/colour-materials-cold.png --materials-cold`. It returns 1 and retains the failing image.
  Repeat on a private GPU using the CNA runner described in `docs/COLOUR_PIPELINE.md`.
  Change the mode to `--materials` for an otherwise matching first-quad repeat.
- **Expected behavior:** The initially bound texture contributes on the first draw, consistently
  with the second draw and matching control shader.
- **Actual behavior:** Cold first sample RGB `(175,152,127)` versus warmed/control
  `(139,120,101)`; remaining stock/control samples and fog endpoints match. Reproduced with
  offscreen Mesa and private hardware-backed Weston/Xwayland. Logs are
  `build/colour-materials-cold{,-gpu}.log`; the warmed modes pass.
- **Workaround:** The diagnostic repeats only its first stock quad before reading calibration
  samples. A cold mode deliberately retains the failure. No blanket extra draw was added to the
  game, and no dependency source was edited.
- **Severity:** Low for the running simulator; medium for reliable first-frame calibration.
- **Blocks future work:** No. Warmed comparisons are usable; production first-frame scope and
  the precise API/state cause still require investigation before attributing a general CNA bug.

## Synchronized presentation is slower in private Weston/Xwayland (cause unconfirmed)

- **Dependency:** CNA runtime / OPENGL33 integration with Mesa and Weston/Xwayland; attribution
  to CNA rather than the driver/compositor has not been established.
- **Affected revision:** CNA `next` at `97783d1e0`; sharp-runtime `next` at `21c06910`.
- **Symptoms:** A GPU-backed High window view averages about 26 ms per draw interval despite
  a 16.667 ms fixed-step target; requesting unsynchronized presentation restores about 16.7 ms.
- **Reproduction:** Use `../cna/tools/platform/run_gpu_tests_private.sh --exec` with
  `env SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy ./build/train-simulator --frames 600
  --route-km 52 --time-scale 0.01 --window-view --quality 2 --no-audio --report-state
  --config build/gpu-profile.cfg`. Compare a second launch adding `--no-vsync`. Observed with
  Radeon 780M, Mesa 25.0.7, private headless Weston and rootful Xwayland. Repeat with
  `--no-avatars` to separate population cost. No live desktop is needed.
- **Expected behavior:** Investigate why synchronized presentation cannot sustain the target
  cadence in a scene that reaches it with the synchronization request disabled; an isolated
  dependency-only reproducer and normal-desktop comparison are still needed.
- **Actual behavior:** With/without avatars, synchronized steady averages were 26.1 ms.
  An isolated unsynchronized repeat with 18 ready avatars averaged 16.7 ms, p95 16.9 ms.
  A later matched 600-frame no-avatar High left-window comparison at 52 km kept VSync enabled:
  fixed timing averaged 26.1 ms (p95 26.9, p99 27.3), variable timing averaged 26.0 ms
  (p95 26.9, p99 27.1), with 419 steady samples each. Disabling the fixed-step wait through
  CNA's public API did not resolve the cadence. Evidence: `build/timing-{fixed,variable}-gpu.log`.
  This does not isolate the remaining driver/compositor/presentation cause.
- **Workaround used by Train Simulator:** Optional diagnostic `--no-vsync` uses the public
  GraphicsDeviceManager setting. Normal gameplay still enables VSync to avoid making tearing
  the default. No dependency source was modified and no automatic retry/reset is added.
- **Severity:** Medium performance concern in this test environment; not an established cause
  of the reported physical-input freeze.
- **Future work blocked:** No; synchronized presentation diagnosis remains open.

## Probable catalog skirt skinning incompatibility with the seated pose

- **Dependency:** CNA Gamer Services avatars; integration with the project's custom seated bones.
- **Affected revision:** `next`, `97783d1e0`, embedded avatar catalog v3.
- **Symptoms:** A seated skirt-wearing passenger exposes an elongated curved hem/loop ahead of
  the knees. Observed in `/tmp/occupied-shaped-seats.png`; the trousers on the neighbouring
  seated passenger do not exhibit it. Attribution to a general CNA bug is not yet proven.
- **Reproduction:** Run `--frames 120 --passenger-view --time-scale 0.01 --route-km 238
  --quality 2 --no-audio` with avatar rendering enabled. Inspect seated people generated with
  skirt clothing. Appearance generation is random, so not every launch reproduces the issue.
  The catalog identifies short/long skirt bottoms as IDs 42/45; both are treated conservatively.
- **Expected behavior:** A seated garment stays near the pelvis/thighs without a large detached
  or stretched hem silhouette.
- **Actual behavior:** The captured hem stretches well beyond the seated legs under the custom
  hip/knee transforms. No isolated dependency-only reproduction has been completed.
- **Workaround:** Project-owned candidate scoring penalizes these known skirt IDs and samples
  at most 128 untouched public `CreateRandom()` results. It prefers trousers without altering
  avatar bytes/assets. The filter checks description format, magic and catalog version; unknown
  versions are unchanged. Bounded random selection is best-effort, not an absolute exclusion.
- **Severity:** Medium visual defect; no observed simulation corruption.
- **Future work blocked:** Realistic seated skirt clothing remains blocked by unresolved pose/skin
  compatibility; other passenger and world work can continue. Dependencies were not modified.

## CNA reserves F9 and F10 globally for renderer context-loss diagnostics

- **Dependency:** CNA runtime
- **Affected revision:** `next` at `97783d1e090c6d84f4b6a6d7746516e65184569e`
- **Symptoms:** Pressing Train Simulator's former `F10` quit binding first asks the renderer to
  restore its graphics context. A long interactive OpenGL 3.3 test emitted a context-recreation
  sequence instead of completing the expected ordinary quit path cleanly.
- **Reproduction:** Bind an application action to `F10`, run it through CNA, and press the key.
  `Game::PollEvents` calls `DebugRestoreContext()` for every non-repeated `F10` key-down before the
  application's `Update` observes the same key.
- **Expected behavior:** Application-visible function keys are either unreserved, the debug action
  is opt-in, or the reserved keys are exposed as part of CNA's public runtime contract.
- **Actual behavior:** CNA unconditionally assigns `F9` and `F10` to renderer context-loss and
  context-restoration diagnostics, so an application binding invokes both CNA's debug behavior and
  its own behavior.
- **Train Simulator workaround:** Quit is bound to `F12`; Train Simulator does not assign gameplay
  or developer actions to CNA's `F9/F10` pair.
- **Severity:** Medium. The collision can unexpectedly disrupt rendering or shutdown during a
  normal application action, but avoiding two keys is straightforward.
- **Future work blocked:** No.

## Overlapping asynchronous avatar construction and first GPU upload can corrupt the heap

- **Dependency:** CNA avatars / OpenGL 3.3 EasyGL renderer
- **Affected revision:** `next` at `9fbe34468c69872204536bfe3dd6f106404d9403`;
  lifecycle-sensitive exit behavior was also observed at
  `97783d1e090c6d84f4b6a6d7746516e65184569e`
- **Symptoms:** During initial population warm-up, the process can intermittently abort with
  `corrupted double-linked list (not small)` or receive `SIGSEGV` inside Mesa allocation code.
  The failure is timing-sensitive and is most visible in short fresh-process station or screenshot
  runs that initialize all 18 avatar visuals quickly.
- **Reproduction:** Construct one `AvatarRenderer` at a time. As soon as its state becomes `Ready`,
  construct the next renderer and then draw the completed renderer for the first time. Repeat for
  the train population under CNA OpenGL 3.3; repeated 45-70-frame offscreen runs eventually fail.
  A captured core showed the main thread in `AvatarRenderer::Resources::Upload`, creating a
  128x128 EasyGL texture, while CNA's avatar loader thread was hashing and building the next avatar
  model.
- **Expected behavior:** A renderer reporting `Ready` can be drawn while a separate renderer is
  built asynchronously, or the dependency serializes any shared work that cannot safely overlap.
- **Actual behavior:** Enqueuing the next model before the previous renderer's first `Draw()` lets
  background avatar construction overlap the first GPU resource upload and can corrupt allocator
  state. The eventual allocation site varies, which originally made screenshot readback appear to
  be the cause.
- **Train Simulator workaround:** Each newly ready avatar, including the final pool member, is drawn
  once outside the camera frustum to complete its first GPU upload. Train Simulator then waits
  across eight complete rendered/presented frames before creating the next randomized avatar. The
  original one-frame interval still produced one rare standalone recurrence under a heavily loaded
  test run; the bounded quiescence window is roughly 133 ms at 60 FPS. Dependency source is
  unchanged. Screenshot readback also uses CNA's
  byte-oriented overload directly to avoid an unnecessary large polymorphic `Color` buffer, but
  that optimization alone did not eliminate the failure. Short lifecycle smoke tests use the Low
  eight-avatar budget. After their requested state frame, they freeze simulation but keep rendering
  until all eight renderers report `Ready`, have completed their first GPU draw, and have crossed
  that quiescence boundary, so the process does not deliberately exit while CNA's asynchronous
  avatar loader or deferred upload is still active. Screenshot runs freeze the requested state,
  complete this invisible loader drain, and capture the fully populated presented frame before
  teardown. The automated suite instantiates
  avatars only in three dedicated lifecycle/render tests, which receive full CTest runner
  exclusivity; unrelated deterministic OpenGL smokes use Train Simulator's `--no-avatars`
  diagnostic mode and remain parallelizable with each other. Normal gameplay and production
  showcase captures continue to use CNA avatars.
- **Additional evidence:** A concurrent full-suite run at the later revision produced one rare
  exit-time `SIGSEGV` after a five-frame smoke test ended with only 6 of 18 requested avatar visuals
  constructed. The isolated test then passed, and 24 additional concurrently launched fresh
  processes completed normally. Two later `ctest -j2` runs each produced one early `SIGSEGV`, but
  in different short OpenGL tests; the first failing scenario then completed 15 times beside a
  second OpenGL process (30 successful fresh processes total). No usable core was available, so
  these occurrences are consistent with the same dependency lifecycle issue under multi-process
  graphics/loader pressure but are not treated as a conclusive second stack trace. A later locked
  full-suite run still produced allocator corruption in one avatar-enabled test and two subsequent
  segmentation faults after 42 of 45 tests had passed. A cross-process CTest lock alone therefore
  did not eliminate the dependency failure. Scoping avatar construction to the three tests that
  explicitly validate it avoids unnecessary loader lifecycles while retaining direct coverage.
  Those three tests subsequently completed 15 repeated fresh-process launches without failure,
  followed by two complete 45-test suites and a normal visible High-quality desktop run with all
  18 visuals ready. A later suite allowed the High avatar test to overlap one no-avatar OpenGL
  process and reproduced an early `SIGSEGV`; the other 44 tests passed. CTest `RUN_SERIAL` is now
  applied to the three avatar tests, because an avatar-only resource lock cannot prevent that
  cross-category graphics-process overlap. Full runner exclusivity alone later saw one standalone
  recurrence after a heavily loaded 41-test prefix. Expanding the post-upload quiescence interval
  from one to eight presented frames then completed ten consecutive 18-visual High launches and a
  complete 45-test suite without failure.
- **Severity:** High without the workaround because it can terminate the application during initial
  avatar population; low with the serialized warm-up.
- **Future work blocked:** No. Twelve consecutive 70-frame fresh-process runs with all station and
  onboard avatar visuals completed after the workaround, as did the normal runtime suite.

## Transient partial renderer-identity removal broke an OpenGL 3.3 consumer build

- **Dependency:** CNA
- **Affected revision:** `next` at `fe20e3e9c23b5353a5b63bbda36f236d799d6612`, with external
  uncommitted edits present in the sibling checkout on 2026-10-06
- **Status:** Resolved later the same day by the ongoing external CNA working-tree update; no Train
  Simulator or dependency workaround was committed. A fresh full OpenGL 3.3 build now succeeds.
- **Symptoms:** Reconfiguring the existing OpenGL 3.3 build removes several renderer enum values,
  then compiling EasyGL fails because `GlProfile.hpp` and `EasyGLRendererDescriptor.cpp` still
  reference `GraphicsRendererType::OpenGLES2` and `GraphicsRendererType::WebGL1`. As the external
  edit progressed, configuration also failed because an old Canvas example remained outside the
  newly declared physical module set.
- **Reproduction:** With the affected dirty sibling checkout, run
  `cmake --build build --parallel 2` in Train Simulator. Depending on the exact point in the
  external edit, either the module audit rejects
  `modules/renderers/canvas/examples/canvas_graphics_capability_test.cpp`, or generation succeeds
  and the EasyGL target fails while compiling the two files above.
- **Expected behavior:** CNA's selected OpenGL 3.3 renderer builds after configuration, regardless
  of removal work for unrelated renderer identities.
- **Actual behavior:** Public renderer enum entries and module declarations are removed before all
  EasyGL references and old source trees are updated, leaving the dependency checkout internally
  inconsistent.
- **Train Simulator workaround:** No dependency patch was applied. During the transient state, the
  last successful OpenGL 3.3 executable remained runnable and the same build tree was temporarily
  configured with `TRAINSIM_BUILD_GAME=OFF` to validate renderer-independent code through ccache.
  Full game configuration was restored after the external CNA edit became internally consistent.
- **Severity:** High while the partial external edit was present; resolved.
- **Future work blocked:** No.

## Intermittent incomplete EasyGL offscreen back-buffer readback

- **Dependency:** CNA, OpenGL 3.3 / EasyGL renderer
- **Affected revision:** `next` at `c7c85c4044159f20f74593ba6a90f1668a317bb4`
- **Symptoms:** After a long accelerated offscreen run, `GraphicsDevice::GetBackBufferData` may
  return a frame with portions of otherwise valid 2D UI glyphs or quads missing. The application
  continues normally and exits cleanly. Repeating the identical run may produce a complete image.
- **Reproduction:** Build the simulator with `CNA_GRAPHICS_RENDERER=OPENGL33`, then run with
  `SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy` for roughly 1,200 frames at an accelerated time
  scale and request a final screenshot. The defect is intermittent. It also reproduces with avatar
  creation and rendering disabled.
- **Expected behavior:** Back-buffer readback after completed draw commands returns the complete
  rendered frame deterministically.
- **Actual behavior:** Some captures contain missing spans of UI content while the rest of the 3D
  frame and UI remain correctly positioned.
- **Isolation performed:** The issue persisted with SpriteBatch atlas text, a project-owned dynamic
  vertex batch, a CPU-composited one-quad UI texture, deep vertex/texture rings, full-buffer
  `Discard` uploads, no avatars, and readback on the draw after the normal presentation boundary.
  This makes a project buffer-lifetime or glyph-generation error unlikely.
- **Train Simulator workaround:** Automated visual checks use repeated short captures, while long
  accelerated soaks validate exit status and state stability rather than pixel-perfect screenshots.
  Screenshot capture is deferred until the draw following CNA's normal presentation boundary.
  The showcase tool writes a temporary candidate, decodes and validates PNG integrity plus the
  expected HUD/map pixel coverage, and retries a rejected readback in a fresh process up to four
  times. Only a validated candidate replaces a committed gallery image.
- **Severity:** Medium for automated visual QA; low for gameplay because an equivalent defect has
  not been observed on a visible desktop window.
- **Future work blocked:** Deterministic long-run offscreen screenshot comparison only. Gameplay,
  headless simulation tests, and ordinary runtime validation are not blocked.
