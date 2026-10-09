# Train Simulator Realism and Production Audit

The focused [screenshot-based environment audit](VISUAL_AUDIT.md) contains 120
visibility-ranked findings from 34 fresh engine captures, plus matched before/after
evidence and performance measurements for the first window and upholstery repairs.

This document is a living audit, not a completion report. It records the evidence-backed gap
between the current playable showcase foundation and the intended realistic passenger-train
experience. Items stay open until they have been implemented, visually inspected, exercised in
the running application, and protected by an appropriate regression check.

## Audit baseline

The October 2026 baseline was assessed from all project source and documentation, the five
authoritative 1280x720 showcase captures, the then-complete 47-test CTest suite (51 tests after
the current audit implementation batches), focused static
analysis, visible X11 keyboard play sessions, and three representative High-quality OpenGL 3.3
offscreen runs.

The representative renderer samples were:

| Scene | Draw calls | Triangles | Loaded chunks | Notes |
|---|---:|---:|---:|---|
| Alderwick terminal dwell | 89 | 210,377 | 7 | station, parked train, road traffic |
| Lakehurst countryside | 73 | 234,935 | 7 | water and far-bank woodland |
| Long tunnel | 73 | 208,530 | 7 | 175 luminaires and 34 safety niches |

The measured 26-30 FPS used Mesa's offscreen software path and is therefore a repeatable QA
baseline, not a claim about normal desktop GPU performance. The current application is playable,
bounded, deterministic, and unusually feature-complete for a procedural prototype. It is not yet
visually realistic.

## What is already strong

- The simulation and renderer are separated well enough that route operation, timetable state,
  seating, passengers, streaming, settings, and most interaction rules are deterministic and
  headlessly testable.
- The complete 286.4 km bidirectional service, twelve stations, terminal reversal, acceleration,
  braking, dwell and door interlocking operate indefinitely.
- Seven 250 m chunks are retained around the train. Large route coordinates do not reach the
  player camera, train interior, or NPC transforms.
- Six passenger cars, five gangways, the accessible toilet, seating, information displays,
  platform-side doors, cab restrictions, the conductor, driver, and station exchange are all
  integrated rather than represented by isolated demonstrations.
- Route-aware audio, captions, pause behavior, tunnel and bridge acoustics, footsteps, door
  events, announcements, murmur, and passing trains share authoritative simulation state.
- Runtime asset discovery, installation, ccache enforcement, renderer selection, input
  diagnostics, screenshot validation, long accelerated service soaks, and dependency-workaround
  records provide a solid engineering base for a much more ambitious presentation pass.

## Principal finding

The refreshed October 8 showcase review still identifies three higher-impact gaps than small
fixture detailing: flat scene-wide lighting, sparse/repetitive landscape and terminal composition,
and visibly stylized stock CNA avatar faces/proportions. Prioritize those foundations while keeping
the unresolved physical-input freeze and frame pacing in scope. The refreshed screenshots are
evidence of the present prototype, not evidence that the realism objective is complete.

The largest gap is rendering quality rather than feature count. At the audit baseline, most of the
scene was built into one vertex-coloured `BasicEffect` vocabulary from boxes, quads, cylinders,
cones, and ellipsoids. Texture coordinates existed in the vertex layout but were zero for project
geometry. The shared effect had texturing, specular response, and per-pixel lighting disabled, and
High did not request multisample antialiasing. The first image-foundation work has now enabled
desktop MSAA and added deterministic mipmapped procedural texture infrastructure. Terrain,
ballast, platform concrete, carpet, and close seat upholstery now use bounded material batches;
the majority of wall, fixture, building, and vegetation surfaces still use the baseline flat
treatment.

This makes correctly modelled details collapse into broad, flat colour regions. Adding more small
boxes without first improving materials, edge quality, depth cues, and silhouette quality has
diminishing visual value. The first production campaign must improve those foundations.

## Findings by area

The synchronized 26 ms cadence is not cured by removing the fixed-step wait. A diagnostic-only
`--variable-step` launch option now selects CNA's public variable-step API; normal launches still
request fixed 60 Hz timing and VSync. Matched private GPU 600-frame High/no-avatar captures at
52 km and effectively frozen route time measured 419 steady samples each: fixed mean/p95/p99
26.1/26.9/27.3 ms, variable 26.0/26.9/27.1 ms. Evidence: `build/timing-fixed-gpu.log` and
`build/timing-variable-gpu.log`. This rejects the proposed simple timing-switch workaround, not
the still-unconfirmed presentation/compositor cause. No dependency source was changed.
Inspection also found preset avatar animation advances a fixed 16.667 ms per draw: that separate
project timing assumption needs correction and regression coverage before variable-step gameplay
could be supported. The new switch is documented as a no-avatar diagnostic, not a performance
preset; it can run uncapped without effective VSync. Final telemetry records `fixed_time_step`.

Leaf-spray mipmaps now preserve alpha-test coverage toward the base texture's visible fraction
at reference alpha 96. A bounded search selects the closest attainable alpha scaling at each
averaged level; RGB and zero-alpha texels stay unchanged. Other materials retain their original
mip path. Deterministic tests verify that correction is never worse than uncorrected averaging,
coverage error remains below four percentage points through 16-square levels, transparent holes
stay transparent and RGB is untouched. Tiny levels cannot encode arbitrary fractional coverage.
A 600-frame moving private GPU run covered 1,559.2 metres and seven world rebuilds with seven
chunks retained (`build/leaf-mip-moving-gpu.log`). Its final image was inspected; no solid alpha
card rectangles appeared. This final still does not prove shimmer-free motion or validate every
view angle. Mean frame interval was 26.0 ms and maximum steady-window interval 55.0 ms, so this
is not a 60 FPS or streaming-hitch fix. Selected regression evidence is `build/leaf-mip-tests.log`.
Dependencies remained clean. Further moving-view inspection and LOD transitions remain open.

Leaf-card placement now shares the exact lobe-layout visitor with the opaque crown. Each tilted
card lies in the outer portion of its own lobe instead of on an unrelated vertical ring. Counts
follow actual crown quality: nine cards/36 triangles on Medium and fifteen/60 on High; Low stays
unchanged. The first follow-up GPU capture removed the detached vertical strips visible in the
initial treatment. An expanded Medium test caught a small upper-height overshoot; card height
was reduced rather than widening the test bound. The final eight selected checks passed in
27.09 seconds (`build/leaf-spray-attached-tests.log`), including a regression that ties every card
centre to the shared lobe layout. GPU evidence is `build/leaf-spray-attached-gpu.png` and matching
log. This fixes placement, not the broader opaque-core silhouette or distant alpha-mip behavior.

The first actual cutout-leaf path is integrated for Medium/High close route-feature crowns:
twelve double-sided cards/48 triangles per crown, one shared streamed batch, a deterministic
128-square 26-leaf texture, and CNA's public AlphaTestEffect with reference alpha 96. The pass
writes depth with opaque blending and copies exterior fog; it uses baked subdued tint rather
than directional lighting. Initial inspection rejected bright, regularly paired sprays that
looked pasted onto the tree. The revised motif uses irregular leaf placement, smaller cards,
inset centres and darker tint. A 60-frame private GPU run completed and its capture was inspected
(`build/leaf-spray-gpu.png`, matching log): alpha holes are visible without solid card rectangles.
The sample reports 92 draw calls, 664,662 triangles and 93,414 combined foliage-material triangles.
Eight selected render/quality/rural/facade/Low-budget checks passed in 27.15 seconds
(`build/leaf-spray-tests.log`); new fixtures protect texture coverage/borders and card bounds/budget.
The source asset is project-generated MIT and recorded in ASSETS. Dependencies were not changed.
This is an initial implementation, not completed canopy realism: some edge-on cards remain thin
or detached-looking, opaque masses still dominate, and generic mip averaging can drop leaves.
Prioritize branch-aligned placements and moving-view/mip validation before increasing density.

Smooth foliage lobes now contract through a bounded object-local radial field rather than remain
perfect ellipsoids. Analytic tangent-gradient normals follow the deformation, avoiding the old
smooth-ball light response on a changed surface. The original topology/UV seam and Low geometry
remain intact. Regression checks cover 0.84–1.0 contraction, outward unit normals, origin
translation and unchanged Low vertices. Seven render/quality/rural/facade checks passed in
22.04 seconds (`build/foliage-surface-tests.log`). The matched 52 km left-window capture
`build/foliage-surface-after.png` was inspected against `build/broadleaf-shape-after.png`.
The surface is less regular, but the gain at passenger distance is small: opaque leaf masses
still dominate. Further small ellipsoid changes have diminishing value; resolving canopy gaps,
branch structure and leaf-scale edge breakup is the next substantive vegetation requirement.
No geometry submissions were added, but mesh-generation arithmetic increased; this is not a
measured performance improvement. The broader realistic-world objective remains incomplete.

Close broadleaf crowns now vary lobe azimuth, radial reach, vertical aspect and tree-wide
width/depth instead of using uniformly spaced rings. Route-feature identities also include the
signed lateral coordinate via the existing stable tree hash, avoiding paired identical crowns
across the railway. High/Medium budgets remain 630/324 triangles per clustered crown and the
existing foliage batch is reused. Expanded bounds tests cover large seeds; a new translated-origin
fixture checks identical relative geometry and different shape across route sides. All seven
selected render/foliage-quality/rural/facade tests passed in 22.06 seconds
(`build/broadleaf-shape-tests.log`). Matched High left-window captures at 52 km were inspected
(`build/broadleaf-shape-before.png`, `build/broadleaf-shape-after.png`): the silhouette changes,
but improvement is modest and the crown still reads as smooth opaque leaf masses. This is not a
completed realistic canopy or a performance improvement claim. A new conversion warning was
corrected and the final incremental ccache build was clean; dependencies remained untouched.

Integration checkpoint after saloon lighting, civilian wardrobe scoring and head-proportion
changes: the complete current CTest suite passed 60/60 in 170.79 seconds
(`build/realism-integration-tests.log`, `LP_NUM_THREADS=4`, serial CTest execution in the existing
build). Coverage includes core repeated-service reversals, interaction smoke, announcements,
station exchange, accelerated rendered terminal reversal, live map, quality/material/facade
checks, avatar population and installation layout. This is automated coverage, not a physical
six-car walkthrough, human audio review or a long real-time rendered memory soak. The reported
physical-input freeze remains unresolved. No additional implementation claim follows merely
from a green suite. Principal table entries for interior lighting, avatar selection/population
and vegetation were refreshed to remove stale earlier implementation descriptions.

Head proportions were further reduced from the earlier 0.82 uniform local-bone scale to 0.74.
The existing joint-preservation/basis/other-bones test was updated; the render, platform, Low
budget and High population checks all passed (23.02 seconds, `build/passenger-head-fit-tests.log`).
Two private GPU OpenGL 3.3 runs completed 180 frames each with 18 ready avatar visuals. Inspected
captures `build/passenger-head-fit-gpu.png` and `build/platform-head-fit-gpu.png` show seated and
standing figures with attached necks, hair, glasses and the sampled native hats. The saloon also
shows the conductor's hat at a distance, which is not a close-up accessory validation. The
appearances are randomized; this is not a matched-identity A/B comparison. The inspected heads
are less oversized, but eyes, facial topology and some garment silhouettes remain stylized.
Further uniform shrinking is not a substitute for improved character assets. The change uses
only the existing project custom-pose path; both dependency worktrees remained clean.

Civilian wardrobe selection no longer lets ordinary hair/accessories compensate for saturated
tops or trousers. The previous top-colour rule admitted almost every stock palette entry, and
its small penalty could still satisfy early acceptance. The scorer now prioritizes restrained
clothing, with a 29-point early-accept threshold and at most 128 untouched public random
descriptions per civilian visual (previously 32). Cream, charcoal, brown, teal and muted purple
remain possible; skin, body and facial features are not scored. Unknown formats remain unranked.
Selection is bounded best effort, not a deterministic wardrobe guarantee or a CNA asset edit.
The core suite passed in 4.05 seconds, including new fixtures proving saturated tops/trousers
cannot reach acceptance through accessory bonuses. A 180-frame private GPU run completed and
its saloon capture was inspected (`build/civilian-wardrobe-gpu.png`, matching log): the foreground
clothes are visibly quieter than the previous bright blue/yellow pair. Random identities differ
between runs, so this is illustrative rather than a controlled same-person comparison. Stock
faces, eyes, hair silhouettes and body proportions remain conspicuously stylized; palette scoring
does not resolve NPC realism. Both dependency worktrees remained clean.

The saloon lighting pass now adds two opposed downward longitudinal diffuse fills, approximating
the broad ceiling strips, with a reduced direct-sun contribution and slightly more neutral ambient
light. Both seat orientations receive fill; the extra lights have no specular component. Opaque
and textured interior batches share the setup, and both lights are explicitly disabled before
each frame's exterior passes. There are no added mesh submissions, shadow maps or dependency
changes. This remains BasicEffect directional lighting, not area-light occlusion or linear-light
shading; avatars still use their own renderer and have not gained matching ceiling illumination.

Matched three-frame High saloon captures at 52 km, fixed time scale and disabled camera sway
(`build/interior-light-before.png`, `build/interior-light-after.png`) were inspected: seat fronts
and rear shells are more legible, while scenery remains unchanged. The improvement is modest;
flat upholstery and missing spatial shadows remain visible. Eight selected render, quality,
facade, MSAA and material-integrity checks passed in 32.54 seconds (`build/saloon-light-tests.log`).
A private GPU OpenGL 3.3 run completed 180 frames with all 18 avatar visuals ready; its capture
(`build/saloon-light-gpu.png`) was inspected. That short startup-inclusive run is not sustained
performance evidence. The existing ccache launcher and four-worker incremental build were used;
both dependency worktrees remained clean. This visual change does not resolve the input report.

The input/streaming probe now rejects event delivery without camera-state yaw movement: each
motion phase requires at least 0.5 radians of wrap-aware travel in new active traces. Its final
state also requires at least three rebuilds, seven retained chunks and 500 metres of travel.
A synthetic frozen-yaw fixture with increasing event counts was rejected; a moving fixture
crossing the angle wrap was accepted. The result receipt now marks a new run in progress rather
than leaving an earlier success visible until exit.

The completed repeat used private GPU Xwayland, High quality, enabled avatars and the default
vsync request. All twelve phases passed: 48 traces spanning 235.702 seconds, 2,399 motion samples,
13 capture acquisitions, four intentional focus losses, one stand transition and a final standing
state. It travelled 16,992.4 metres, rebuilt the world 68 times, retained seven chunks and reported
18 ready avatar visuals. Evidence: `build/input-streaming-yaw-probe.log` and
`build/x11-input-test/{game.log,result.txt,phase-*.png}`. Selected phase captures were inspected.
An earlier attempt was invalidated by editing the shell script during execution; its exit 127
was a test-operation error, not a simulator failure. The repeat ran the script unchanged and
exited zero. The core CTest also passed (2.79 seconds); no application recompilation was needed.
This does not certify physical mouse delivery, all-car walking, audio or a long play session.
Five-second traces cannot exclude brief freezes. Camera-state movement is stronger evidence
than event counts, but is not a frame-by-frame rendered-camera check. The physical-input issue
remains open. The synchronized sample still reported 26.0 ms mean frame time and a 58.4 ms
maximum in the steady window; no 60 FPS or hitch-free claim is warranted.

A current High-quality GPU streaming audit (1,800 frames from 52 km, time scale 2.4, 18 ready
avatar visuals, audio off, vsync request off, private Xwayland) covered 3,135.9 m and 13 world
rebuilds. Steady-frame mean/p95/p99 were 16.9/16.9/17.0 ms, but the maximum was 59.4 ms. World
rebuild maximum was 46.2 ms; generation/upload totals were 303.8/210.6 ms. This demonstrates
short streaming hitches, not a mouse-capture lock. Evidence: `build/streaming-gpu-audit.log`.
Broadleaf and needle staging vectors, previously recreated every transition, now retain capacity
and are cleared before each rebuild. A unit test protects retained capacity, empty geometry after
clear, and fresh indices after reuse. This is a limited allocation optimization with retained peak
CPU memory, not a claim that synchronous world streaming or the reported camera freeze is fixed.
The repeated private GPU run covered 3,134.1 m and the same 13 rebuilds with 18 ready avatars.
Steady mean/p95/p99 were 16.9/16.9/17.1 ms; the maximum was 54.7 ms. Rebuild total/max were
484.4/45.0 ms; generation total/max 284.4/26.9 ms and upload total/max 199.9/24.1 ms
(`build/streaming-staging-gpu.log`). These are one before/after pair with slightly different route
advance and runtime scheduling, not a statistically isolated speedup. Typical frame time remains
unchanged and the residual 45 ms rebuild exceeds a 60 Hz frame budget. WLD-003 stays open.
Nine selected core/render/foliage/rural/waterfront/facade checks passed in 25.43 seconds
(`build/streaming-staging-tests.log`). Dependency worktrees remained clean; the build emitted
only the existing sharp-runtime `__int128` pedantic warnings in included headers. Neither GPU run
injected mouse input, so this evidence must not be used to close the input-freeze report.

Diagnostic component colours identified the dark crop rectangle: magenta soil remained hidden,
while red row geometry changed the entire dark region and cyan perimeter hedges remained separate.
The culprit was the rows' long shaded vertical box walls, not the soil tint. Temporary colours
were removed. Rows now use continuous triangular canopies with rendered-terrain samples every
2 m or less, buried roots and deterministic height variation. The first narrow/tall profile
still read as a dark barrier; the production young-crop proportions are now 0.48 m wide and
0.26 m high before scaling. A 60 m row costs 122 triangles versus 72 previously, but keeps the
existing static-world submission. Unit coverage checks terrain-relative heights, positive
shoulder normals, dimensions, empty input and topology budget. This remains a simplified crop
canopy, not individual stalks or an exact terrain-triangle-clipped volumetric surface.
The final same-view capture (`build/crop-rows-after.png`, High left window at 52.125 km) was
inspected against `build/field-hedges-after.png`: the former solid dark wall now reads as low
individual rows with visible gaps. Final state reported 91 draws and 660,582 triangles for the
whole scene. Eight selected checks passed in 22.55 seconds (`build/crop-rows-tests.log`). The
final build has no diagnostic colours and both dependency worktrees remain clean. This is a
verified improvement in the captured view, not full-route crop realism or a sustained FPS claim.

Crop-field hedges now follow the actual rendered terrain via one continuous profile per edge,
rather than six horizontal pieces positioned at analytic midpoint heights. Every profile vertex
uses the terrain sampler, with an 8 cm buried base. The longitudinal mapping reverses its local
axis to preserve surface orientation; tests cover both edge directions, ground clearance,
profile extent, upward crown normals and the bounded mesh budget. This removes internal hedge
caps and overlap steps without adding a render submission. The crop-row boxes still need the
same terrain-placement attention; soil and perimeter work do not certify them.
Eight focused checks passed in 22.77 seconds (`build/field-hedges-tests.log`), and the High left
window capture at 52.125 km was inspected (`build/field-hedges-after.png`). The perimeter no longer
uses midpoint-flat segments, but the grazing view still presents the crop parcel as an overly
dark, block-like area. That visual problem remains open and needs a closer row/material audit;
the conformance tests must not be read as proof of convincing crop appearance. The incremental
build was warning-free and both dependencies remained untouched.

The crop soil surface no longer spans an entire parcel with a single quad sampled from analytic
terrain heights. It is clipped against each underlying terrain cell and its actual a-c triangle
diagonal, then lifted 18 mm. This addresses a concrete geometry mismatch: the analytic surface
and a coarse rendered triangle are not generally coincident, and even a smaller rectangular
quad can bridge the source diagonal. A non-planar fixture checks triangle-centroid heights,
upward normals, crop bounds, covered area and empty bounds. Crop row boxes and their enclosing
hedges are explicitly not fixed by this change; their older ground sampling is a follow-up item.
Eight selected render/rural/foliage/waterfront/facade checks passed in 22.52 seconds
(`build/crop-ground-tests.log`). Both window directions were captured at 52.125 km and inspected
(`build/crop-ground-after.png`, `build/crop-ground-left.png`). The view is too grazing/distant
to certify every field edge; existing row/hedge silhouettes obscure much of the soil surface.
The exact conformance evidence is the non-planar geometry fixture, not an overstated screenshot
claim. Both dependencies stayed clean and the incremental build completed successfully.

Rural ground tint now represents shared agricultural parcels instead of choosing one of four
similar greens separately for each lateral terrain band. Absolute 500 m route cells match the
existing transverse hedge rhythm; inner/outer field identities and route side select six muted
pasture/hay/stubble/fallow tones. The maintained railway corridor and distant haze retain their
previous colours. No geometry, texture or draw-call cost is added. Unit checks span 24 parcels on
both sides, require same-field consistency and all six palette entries, and protect corridor and
haze values. The High 52 km capture was inspected (`build/rural-parcels-after.png`): the existing
hedge now separates visibly different fields. Longitudinal boundaries are still incompletely
modeled, and the field-crop overlays need their own terrain-conformance audit; this is not a
complete agricultural environment.
Eight selected render/foliage/rural/waterfront/facade checks passed in 22.67 seconds
(`build/rural-parcels-tests.log`). The incremental build had no warnings and the dependency
worktrees remained clean. This batch has an offscreen running-image check, not a new hardware
performance or physical-input measurement.

Conifer feature identity now includes stable absolute route and lateral position, mixed with
species, instead of reusing only species values 0 and 3 for every near crown. This changes branch
azimuth/reach and adds bounded crown breadth and interior-whorl spacing variation without extra
vertices, submissions or footprint expansion. Tests cover coordinate-bin stability, distinct
neighbour/opposite-side/species identities, actual mesh differences, and larger mixed seed values
in the existing geometry bounds/normal/budget checks. The hash intentionally excludes floating
origin and streaming state. Horizon/lake-bank decorative populations are not covered by this
identity migration; physical aging and stronger species diversity remain open.
Seven selected render/foliage/rural/facade checks passed in 25.99 seconds
(`build/tree-identity-tests.log`). The running High 52 km capture was inspected
(`build/tree-identity-rural.png`): neighbouring conifers have different crown/branch patterns,
although their shared low-poly construction remains obvious. Build output had no warnings and
both dependency worktrees stayed clean. This is not evidence of a camera-freeze fix.

Near conifer fans now receive an opaque, procedurally generated 128-square needle-spray material
on Medium/High. A full mip chain filters the fine crossed sprays; broad periodic clumps modulate
the otherwise uniform green surfaces. Tree-local dominant-face UVs use a physical 0.75 m period
and are checked for origin independence. All near conifers share one additional streamed material
submission, with no extra triangles or alpha sorting; Low and distant LODs are unchanged.
The High 52 km running capture was inspected (`build/needle-material-rural.png`): surface breakup
is subtle at normal window distance, and large polygon facets remain plainly visible. This is
material support, not completion of the vegetation realism work. Seven selected render/foliage/
rural/facade checks passed in 23.04 seconds (`build/needle-material-tests.log`), including new
neutral-colour, determinism, contrast, UV and mip-chain checks. Asset provenance is project-created
MIT and documented in `docs/ASSETS.md`.
The same High view also ran successfully on the private GPU-backed OPENGL33 path and its image
was inspected (`build/needle-material-gpu.png`/`.log`): 91 draw calls, 89,334 combined foliage
material triangles, saved capture confirmed. This three-frame check does not establish sustained
frame rate or physical-input reliability. Both dependency worktrees remained clean.

The close-conifer silhouette no longer consists of seven continuous cone skirts. Each whorl
now has independent drooping branch fans and a narrow central needle mass, with deterministic
variation in direction, reach, droop and muted colour. The urban window capture at 0.783 km was
inspected (`build/conifer-fans-after.png`): branch gaps break the stacked-cone outline. Geometry
cost is unchanged at nine branches (504 triangles), no draw batches are added, and distant LODs
remain unchanged. Tests cover world-origin independence, reproducibility, bounds, normal lengths
and the exact bounded budget across five requested detail levels and four seeds. The result is
still visibly stylized; needle detail, stronger age variation and LOD transitions remain open.
Seven selected render/foliage-quality/rural/facade checks passed in 22.13 seconds
(`build/conifer-fans-tests.log`). The additional High rural capture at 52 km
(`build/conifer-fans-rural.png`) was inspected: near crowns retain distinct branch silhouettes,
but broad flat fan surfaces are still conspicuous. Finer material breakup is needed before
calling these realistic. The final incremental build was warning-free; dependency worktrees
remained clean. These captures do not establish a physical-GPU frame-rate improvement.

Station window proportions now share the corrected platform datum with the entrance. The old
small-station layout squeezed two rows into 4.8 m measured from rail level; it now represents one
tall public waiting-hall floor. Major/terminal facades retain three rows with 2.91/3.24 m pitches
above the platform, and all window apertures are 1.45 m high. The ground-floor sill is 0.925 m
above the platform rather than nearly at paving level. The close Stonebridge view was inspected
(`build/station-storeys-after.png`): window heads now align naturally with the entrance surround.
An additional layout defect was corrected: the omitted ground-floor column was always on the
same side even when the door offset changed sign. Clearance now uses the actual door position,
window/frame width and a 0.25 m wall pier. Unit checks cover all three building envelopes, roof
clearance and both entrance offsets. Station interiors remain unmodeled and this view does not
certify all rear/end elevations.
All six focused render/platform/material/facade checks passed (21.33 seconds,
`build/station-storeys-tests.log`). A second running capture at Merefield with the opposite
entrance offset was inspected (`build/station-storeys-merefield.png`); the formerly conflicting
bay is now reserved for the door and the adjacent window remains separate. CNA and sharp-runtime
were not modified. No new input-stability claim follows from these rendering checks.

A closer Stonebridge capture (`--station 7 --car 3 --platform-view`) now shows the main station
front rather than the separate clock turret seen from car 5. The new plaster shell is continuous
behind the window and door fixtures in that view; roof/end-wall coverage still needs other angles.
The former two-box entrance has been replaced with framed paired leaves, glazing, protective
lower panels, safety bands, threshold and pull handles. Reviewing the first new capture also
exposed an older placement error: rail-level door bases were buried by the 0.78 m platform.
The entrance base now follows the platform surface plus 4 mm, preserving its intended height.
The component uses the existing opaque batch and remains scenery, not an exit. Geometry tests
check bounds, hardware projection and both side orientations. Before the final sill-elevation
adjustment, all 60 CTest checks passed in 170.87 seconds; final placement checks are recorded below.
After rebuilding the corrected platform-level placement, all six focused render/platform/High/
material/facade checks passed in 21.37 seconds. The final close capture
`build/station-entrance-after.png` was inspected: the threshold meets the paving and the complete
lower panels and handles are visible. Logs: `build/station-entrance-full-tests.log` and
`build/station-entrance-final-tests.log`. Dependency worktrees remained clean. This does not
validate physical input or resolve the intermittent camera-freeze report.

Residential and station material coverage now wraps all four wall elevations rather than only
the long passenger-facing wall. A first end-wall overlay attempt exposed depth competition
with duplicate solid building bodies, particularly at grazing angles in the opt-in shader.
Those bodies are now replaced by the material wall shell itself; roof and plinth/ground
geometry remain separate. This avoids a larger offset or polygon-bias workaround, preserves
physical two-metre texture scale and vertical courses, and keeps the two existing material
submissions. Houses, townhouses, apartments and station buildings are covered; towers and
industrial buildings retain their existing materials. Unit checks cover transformed end-wall
normals, dimensions, UVs and invalid dimensions. Six focused core/render/image tests passed
(24.65 seconds); the High private GPU image comparison also passed, with unchanged tolerances.
The urban image was inspected: end walls now retain masonry detail in both stock and optional
linear-light rendering. Sparse urban layout, plain rooflines and stylized vegetation remain
major realism gaps; this does not close the overall visual audit.
Additional Stonebridge captures exercised the changed station scene successfully, but the
platform-facing framing shows only the edge of the station building. They do not certify all
station wall elevations; a dedicated building-angle inspection remains outstanding.

The facade colour experiment now has opt-in in-game paths (`--facade-control` and
`--linear-facades`), confined to masonry and plaster batches. The fixed urban view was inspected
at Medium and compared numerically at Medium/High. An initial depth-integration error hid the
overlays behind building bodies; explicitly converting XNA clip depth to OpenGL fixed it
without changing CNA or weakening depth/culling. Default rendering is unchanged. The control
mean RGB error is under 0.13 byte levels in these views; linear-light shaded facades visibly
retain more detail. This is a reversible material integration, not a claim that the sparse
urban environment now looks realistic. Image regression checks cover visibility as well as
batch execution. See `docs/COLOUR_PIPELINE.md` for commands and remaining validation limits.
The private GPU-backed High comparison also passed after stabilizing the test camera (vibration
off, negligible route advance); the first moving-camera comparison was invalidated by different
startup timing. This is image-validation evidence, not a claim of physical mouse testing or a
fix for the still-open intermittent camera freeze.

The colour experiment now covers the actual masonry texture, the game's directional daylight,
three surface normals and three fog depths. The stock/control mismatch initially came from
CNA's documented non-MSAA XNA pixel-centre displacement: the custom shader sampled a neighbouring
texel at mortar boundaries. Matching that displacement makes the control agree, without any CNA
change. All 27 warmed material patches pass on offscreen Mesa and private hardware OpenGL33;
a representative wall changes from `(96,88,79)` to `(123,109,94)` under linear-light calculation,
while the fully fogged endpoint stays fixed. The material image was inspected.

An independent first-use textured-draw discrepancy remains reproducible in a cold mode and is
recorded in `bugs.md`; the warmed comparison repeats only the first stock quad. The original
untextured numeric reference and the UV-ramp control also pass. This evidence supports trying a
reversible exterior-material integration, not globally gamma-correcting the final game image.
Production rendering remains unchanged; filtering/mips, transparency, cabin materials and avatar
consistency are not certified by this fixture. Details are in `docs/COLOUR_PIPELINE.md`.
The unchanged production core/render/interaction/High checks passed again (4/4, 6.78 seconds);
both dependency worktrees remained clean.

The colour-space hypothesis now has a project-owned executable check rather than source-only
inference. `trainsim_colour_probe` renders actual CNA BasicEffect patches alongside a direct-
multiply control shader and an sRGB-decode/linear-light/sRGB-encode shader. All 27 patches (81 RGB
channel checks, tolerance two bytes) passed on offscreen Mesa and private GPU-backed
Weston/Xwayland. Neutral grey 128 at half illumination reads 64 through BasicEffect and the
control, versus 92 through the linear-light calculation. The three-row image was inspected.
See `docs/COLOUR_PIPELINE.md` for reproducible commands and the interpretation boundaries.
No production effect, palette, HUD, avatar or camera behaviour changed in this experiment;
normal-based exterior lighting, textures, fog and blending still need controlled comparisons.
The existing core/render/interaction/High smoke checks also passed (4/4, 6.81 seconds) after
building the diagnostic. The tool is excluded from default builds and installation.

Public-court infill now excludes a previously missed conflict: paths to farther-back houses can
cross the court even though their building footprint does not. Placement reserves the current
house/townhouse/apartment/tower entrance offsets, widths and bounded pavement-connection range,
on both route sides and across loaded chunk boundaries. Regression cases distinguish same-side
crossing paths from opposite-side, displaced and out-of-range entrances.

Two low planted beds now occupy the court's rear edge, with buried footings, visible soil and
six restrained shrub crowns; the centre and bench approaches stay clear. The bounded court mesh
is now 434 triangles in the existing batch. Six focused tests passed in 11.27 seconds. The matching
High left-window km-0.783 image (`/tmp/street-pocket-planted.png`) was inspected against the bare
court: both beds are visible, the pavement connection remains clear, and four courts are loaded.
This adds residential infill detail but does not solve the wider sparse district composition.
The subsequent complete regression run passed all 58 tests in 187.46 seconds, including service,
input/interaction smoke coverage, audio, avatar population, material captures and installed layout
(`build/urban-infill-full-regression.log`). This does not certify the unresolved physical mouse
freeze or sustained desktop frame pacing.

The next image-foundation investigation is colour-space/lighting calibration, not more street
furniture. Read-only inspection at CNA `97783d1e0` confirms its OpenGL33 stock `Lit3DSource`
multiplies texture, diffuse lighting and vertex colour directly, then fog-blends the result;
that BasicEffect path has no explicit output sRGB transfer. The PbrEffect source instead exposes
texture decoding/output encoding controls. The simulator currently authors its palettes as
eight-bit colours and uses BasicEffect, so its lighting numbers must not be treated as a verified
linear-light workflow. This is not evidence of a CNA bug (the legacy XNA-compatible path is
intentional), nor proof that gamma alone causes the flat appearance. A controlled project-local
material/lighting comparison is needed before changing the global palette or increasing ambient
light again. Dependency code remains immutable.

The first larger-gap treatment now inserts small paved public courts between compatible
townhouse parcels, rather than adding details to every facade. Each court connects to the street
pavement and has two facing benches. Deterministic placement reserves building/vegetation bounds,
rejects station approaches and unsuitable terrain, and conservatively checks neighbouring chunks.
This is limited to Medium/High and reuses the static world batch (242 triangles per court).

Six core/render/urban/High/material/Low checks passed in 16.12 seconds. Runtime telemetry at km 0.9
reported four courts across the seven loaded chunks. That initial view did not show a court;
the left-window capture at km 0.783 (`/tmp/street-pocket-left.png`) then confirmed the paved area,
both benches and its pavement connection without a visible building overlap. The wider district
remains sparse and repetitive; courts still need more convincing landscaping and the placement
model does not yet solve all intersections with distant-house entrance paths. Unit checks cover
feature-order independence, occupied/narrow-gap rejection, mirrored winding and geometry bounds.
No dependency source was changed.

The townhouse frontage review found a genuine placement defect: the street setback reserved
only the building's half-width, although its raised garden plot is wider. The full plot is now
kept beyond 25 m from the route centre, preventing its edge from spilling onto the outer pavement.
Near Medium/High parcels gained low masonry/metal front boundaries with an entrance-aligned gap
and narrow soil/planting strips. The new detail stays inside the reserved footprint, leaves the
path clear, shares the existing world batch and is omitted on Low/beyond 500 local metres.
The fixed plot-top height is preserved when the building is scaled.

Six core/render/urban/High/material/Low checks passed in 10.78 seconds. The matching High km-0.9
capture `/tmp/townhouse-garden-after.png` was inspected against the preceding facade view: fronts
read as bounded residential parcels and the entry paths remain visible. This improves near-road
composition, not the still-overlarge uniform lawns between independent buildings. District-wide
parcel connectivity and land-use variation remain open. Dependencies were not modified.

The km-0.9 city review exposed a second facade-proportion defect: townhouse height variants all
kept three window rows at the same heights, leaving a conspicuous unused upper wall. They now
have three/four/four storeys, evenly related to wall height, on front and end walls. Portrait
1.35 m panes replace the undersized 0.86 m front panes; the second-row planter follows the new
sill rather than a fixed elevation. Building footprint, roof, door and path dimensions are
unchanged. Tests check nominal storey pitch, frame/roof and ground clearances across twelve
variant identities and three scales. Both iterations were captured and inspected at the same
route position: `/tmp/apartment-district.png` is the prior facade, `/tmp/townhouse-storeys-after.png`
isolates row placement, and `/tmp/townhouse-proportions-after.png` includes the final pane change.
The foreground now has a more coherent storey/window rhythm, but repetitive free-standing blocks
and uniform grass between plots remain larger composition problems. No new material batch was
introduced; additional upper-row windows do add geometry in taller variants.
All five final render, urban, High/MSAA, material and Low-budget checks passed in 12.67 seconds.
Builds used the existing tree, system ccache and four workers; dependency worktrees stayed clean.

Apartment blocks now receive a roof-height projected ground silhouette in Medium/High, sharing
the residential shadow batch and the dimensions used for their opaque body/roof. Their raised
forecourt is included in the receiver; Low retains its previous shadow policy. The four height
variants also exposed a facade defect: all used five window rows even when the wall grew from
13 to 19.6 metres before scaling. Passenger-facing and end-wall rows now range from five to eight,
with tests for roof/frame clearance at three scales and twelve variant identities. Towers and
warehouse compounds are deliberately not included in the residential shadow approximation.

The new High km-0.9 capture (`/tmp/apartment-district.png`) was inspected: the taller apartment
at the left has windows extending to its upper storeys and a restrained ground projection.
The km-63.77 and km-2.6 captures mostly show row houses; they are not evidence for apartment
improvement. The district remains repetitive and visibly procedural, and this is not general
shadow mapping. The full 58-test suite passed in 142.91 seconds after the shadow change, before
the subsequent facade-row correction. After that correction, all five focused render, urban,
High/MSAA, material-integrity and Low-budget checks passed in 8.22 seconds. Both builds were
incremental with system ccache and four workers; dependency worktrees remained untouched.

The 26 ms GPU-backed result was narrowed with four 600-frame High runs at route km 52 and
time scale 0.01 on the same private Weston/Xwayland path. Default synchronized presentation
averaged 26.1 ms after warm-up both without avatars and with 18 ready visuals; their p95 was
26.9 ms. With the diagnostic `--no-vsync`, the no-avatar steady mean became 16.7 ms (p95 16.8).
An isolated avatar-enabled repeat also averaged 16.7 ms (p95 16.9, p99 17.0, maximum 17.1 across
419 steady samples). The initial avatar/no-VSync run overlapped regression testing and was
therefore repeated alone rather than used as the clean comparison. The scene is not perfectly
frozen: slow route progress produces a small difference in culling counts. This does not prove
which layer causes the synchronized delay, but argues against reducing avatar or scenery quality
as the first response. It is not a general 60 FPS claim for all scenes or hardware.

The new launch flag retains the 60 Hz simulation target, is not persisted and leaves default
VSync on. Final telemetry records the request explicitly; two smoke tests cover default/override
routing. Five core/render/interaction/option checks passed (13.94 seconds). An unresolved
integration issue with exact reproduction and dependency revisions is recorded in `bugs.md`.
Logs are `build/gpu-profile-{no-avatars,avatars,no-vsync,avatars-no-vsync-repeat}.log`.

The input audit now covers a second display path: private headless Weston plus rootful Xwayland
on the actual AMD Radeon 780M (Mesa 25.0.7, GLX reports accelerated). CNA still selects OPENGL33;
the driver's reported GL 4.6 capability does not change the application renderer. The new opt-in
probe backend refuses an unverified/caller desktop display, and records its renderer and quality.
A twelve-phase High run with avatars passed: 48 input traces span 235.564 seconds, with maximum
trace spacing 5.025 seconds, 2,399 nonzero motion samples, 13 capture acquisitions, four deliberate
focus losses, and successful standing/short walking at the end. Initial/final-phase images were
inspected. There was no reproduced mouse freeze in this synthetic run, but it is not physical
mouse, native SDL Wayland, six-car traversal or multi-hour evidence.

This GPU-backed run also establishes a performance concern separate from input delivery. The
last 2,048 frame intervals averaged 26.0 ms (p95 26.8, p99 30.5, maximum 56.0); the whole-run
maximum was 99.2 ms across 9,207 intervals. Seven chunks remained loaded through 69 rebuilds,
with 330 recorded buffer reuses. Maximum world rebuild was 52.5 ms. These are private-compositor
end-to-end timings, not isolated GPU costs or normal-desktop FPS certification; presentation,
simulation and renderer CPU cost still need separation. Evidence is retained in
`build/x11-input-test/xwayland-high-12-passed.log` and `xwayland-high-renderer.txt`. No dependency
source changes were made. The physical-input freeze remains open.
The default three-phase Medium/no-avatar Xvfb path was rerun after the backend extension and
also passed (458 motion samples). Invalid backend values and attempts to bypass the private
compositor environment were explicitly rejected with status 2. Next performance work should
separate compositor/presentation pacing from simulation, avatar and scene costs before lowering
visual quality in response to the 26 ms result.

Terminal terrain now separates a muted gravel/service margin, drier intermediate grass and
managed urban green through bounded vertex tinting. The existing terrain texture, geometry and
draw batches are unchanged. Influence is full within 350 m of either endpoint and smoothly fades
to the previous urban tint by 600 m; the inner railway corridor and terrain beyond 260 m lateral
distance are unchanged. Tests cover those exclusions, both endpoints, small-distance continuity
and gravel/grass colour separation. Four render/High/material/Low checks passed (7.25 seconds).
The matched Alderwick seated capture was inspected: the broad pale maintenance band is now
distinct from the lawn, but the sparse composition and coarse interpolation remain visible.
This is colour zoning, not a new physically based terrain material or completed landscape.

Terminal midground planting adds deterministic paired broadleaf groups between the railway and
the new street, confined to the authored city extension beyond each route endpoint. This avoids
overlap with in-route streamed buildings and traffic. The platform-building side excludes the
first 90 m; crowns and soft ground shadows remain within the 40–100 m terrain band and clear of
the sidewalk. Tree roots and shadow receivers use the same rendered-triangle sampler as the
street. Medium uses reduced crowns; Low omits the addition. Ten candidates per side bound the
cost, with station exclusions reducing the actual count. Regression coverage checks deterministic
placement, root separation, scale and pavement clearance. Four render/High/material/Low tests
pass (7.61 seconds). The matched Alderwick window image was inspected: nearby tree groups add
a depth layer while preserving facade views, but the foreground remains sparse and the stock
procedural crown silhouettes are still stylized. This is not a finished urban landscape.
After this pass, the complete current 56-target CTest suite passed in 123.88 seconds, including
service reversal, interaction, audio, population, material captures and install-layout checks.
This validates their covered regressions after the accumulated lighting, wardrobe, upholstery
and terminal changes; it does not establish real-mouse reliability or complete urban realism.
Both dependency worktrees remained clean.

Terminal composition now includes bounded six-metre entrance terraces with paving joints,
curbs and planted ends at the skyline blocks, plus a 650 m street on each side at 58 m from
the tracks. Seven-metre roadways have two 2.6 m sidewalks and dashed centre markings. Street
vertices sample the rendered 50 m terrain triangles in the existing 40–100 m bands; the current
route endpoints are aligned to that grid. Medium/High reuse the static world batch; Low is
unchanged. Tests cover mirrored terrace bounds, clear central entrance space, street triangle
budget and ground-relative heights. Four render/High/material/Low checks passed (7.34 seconds).
Matched Alderwick window captures were inspected: the added frontage and street are visible,
but at passenger-eye height the road is a narrow band and the broad empty railway-side verge
still dominates. The initial terrace-only result was too weak, prompting the street addition.
This is incomplete urban composition: side access connections, a continuous wider street network,
and populated midground remain missing. The new streets have no traffic paths yet. The first
Westhaven window check was occluded by a parked train, not evidence of the street there.
The opposite seated view was also blocked by station architecture. A separate 286.2 km approach
capture exposed the skyline frontage behind the existing near-road buildings; the new pavement
is visible there, but only part of its length is in view. Full endpoint-area composition remains
an open visual-review task rather than being certified by these few frames.

Civilian appearance selection was audited after a bright cap/bucket-like silhouette in the
saloon capture read visually as protective headgear. It was not established to be an actual
helmet asset. The old scoring gave ordinary headwear more points than uncovered hair. The new
read-only candidate scorer reverses that preference and gives broad bucket/bright headwear a
penalty that unrelated clothing points cannot offset. Subdued ordinary hats remain possible;
skin tone, body type and facial features are not selection criteria. Selection still uses at
most 32 untouched CNA random descriptions, so it is a preference rather than an absolute ban.
The complete civilian scorer now checks description length, magic, format and catalog v3 before
inspecting offsets; unknown descriptions receive no preference. Previously only the skirt-risk
subfilter had that full guard. Regression fixtures cover hat preference, bright/bucket penalties,
seated-skirt compatibility and unknown/truncated formats. This fixes project selection logic,
not CNA, and does not change the stylized stock face geometry.
Core/render/Low-budget/population tests passed (22.14 seconds). A fresh avatar-enabled 70-frame
saloon capture was inspected and passed HUD validation, with ordinary uncovered hairstyles in
the foreground. One random sample does not establish the long-run distribution or guarantee that
all future selections lack hats. The README saloon image was refreshed; both dependency worktrees
remained clean.

The interior material review found that seat fabric reused the carpet texture. Upholstery now
has a separate fine woven source, 16 cm physical repeat and low-contrast staggered yarn dashes.
Backrest UVs are seat-local and cushion mapping uses the same scale. Six render/interaction/
sunlight/High/material/Low-budget tests pass (14.40 seconds), including deterministic image,
mip and translation-invariance checks. Matched no-avatar car-2 views were inspected: the seats
now have a distinct restrained textile pattern rather than the carpet's diagonal grain. Geometry,
collision and total fabric triangles are unchanged; the split adds one static draw and one small
mipmapped texture. It does not resolve the stock avatars, sparse exterior, or all interior-lighting
limitations, and the no-avatar comparison is not evidence of a complete passenger scene.
An additional 70-frame avatar-enabled saloon capture was inspected, passed HUD validation and
replaced the README saloon image. It still exposes the much larger realism gap in stock avatar
faces and randomly selected accessories; the fabric change should not be mistaken for resolving it.

Platform luggage and handheld props now receive canopy shading as well as their owners.
Luggage uses its own position and a 0.45 m-above-platform receiver, while phones and the staff
band follow the owner's torso sample. The shared BasicEffect lighting is explicitly restored
after platform drawing. Four render/interior-sunlight/platform/population tests pass (16.27
seconds); the avatar-enabled Stonebridge capture was inspected. A fixed-view image regression
now compares clear paving patches under the roof and in the sun. The previous unprojected-shadow
capture had a brightness ratio of 0.999 and is rejected; the projected-shadow view measures
0.697 and passes. The check also rejects implausibly black receivers. This protects the visible
ground contrast, not the correctness of every prop's lighting. Static benches and arbitrary
station meshes still lack canopy occlusion.

Station canopy shade now follows the actual sunlight direction instead of two weak rectangles
centred directly under the roof. The projected ground footprint is clipped to the platform and
uses the existing shadow batch. Waiting avatars share that projection at torso height, retaining
ambient light while suppressing most direct sun. Tests cover the receiver-height shift, opposite
light directions, an edge blend and invalid/above-roof receivers. Five render/platform/High/
material/population tests passed (18.50 seconds). The avatar-enabled Stonebridge capture was
inspected: a clear sun/shade boundary now anchors the canopy to its platform, and waiting people
no longer retain full sun highlights under it. Avatar appearances vary randomly across runs, so
this is not a matched-skin pixel comparison. Fixtures and luggage do not yet receive this roof
occlusion; whole-avatar lighting remains an approximation rather than per-pixel shadowing.

Close conifer crowns now use seven staggered branch whorls instead of four large triangular
skirts. Each skirt has a bent shoulder and irregular drooping tips, reducing the oversized
layered-cone silhouette. The implementation is shared with geometry tests that check repeatability,
origin translation, bounds, unit normals, index count and zero-scale rejection. A nine-sided
close crown costs 504 triangles rather than 144; the distant three-cone crown remains 54 triangles
at nine sides. No new material or draw batch is needed. Four render/High/material/Low-budget checks
passed in 7.28 seconds and a matched 63.77 km window view was inspected. The crown is finer and
more continuous, but still visibly stylized; opaque skirts do not reproduce real needle sprays,
branch gaps or self-shadowing. This is an intermediate geometry improvement, not a finished tree
asset or a measured frame-rate improvement.

Exterior lighting now uses less ambient fill and a stronger, warmer directional sun, shared by
plain/textured world batches and platform avatars. The summed daylight intensity and full-tunnel
endpoint are preserved; cabin lighting is unchanged. A matched 63.77 km before/after window view
was inspected: shaded building sides have stronger cool/warm separation, although the improvement
is modest and does not resolve the scene's sparse composition or stylized vegetation. No new
geometry, draw batch or shadow pass was introduced. Regression checks cover intensity balance,
interpolation and clamping. Six focused render, High-quality, material-capture, tunnel, interior
sunlight and avatar-population tests passed (19.23 seconds). This pass is not a claim of physically
based lighting or a resolved realism objective.
The subsequent complete 56-target CTest suite passed in 116.18 seconds. A separate avatar-enabled
Stonebridge platform capture was visually inspected: clothing and faces remain legible, but
their stock stylization and the absence of canopy occlusion remain obvious. The dependency
working trees remained unchanged.

The input audit found that existing five-second traces omitted help/display inspection and
actual time between records. These omissions could hide intentional look suppression or make
a stalled update loop look like lost mouse delivery. Traces now append those overlay flags,
drawn-frame count and measured wall-time interval (zero for the first record). The private X11
probe checks field presence, advancing frame counts and valid intervals. This change does not
alter capture behavior and is not a repair claim for the unresolved physical-input freeze.
After the diagnostic-only application change, core/render/interaction checks passed again.
The three-phase private X11 probe passed with 476 observed nonzero motion samples and valid
frame/interval fields, exercising pause, map, focus recovery, standing and a short walk. The
first probe completed gameplay but its new AWK assertion had a line-continuation syntax error;
that assertion was repaired, checked against the saved log, and the entire probe rerun with
exit status zero. This approximately one-minute synthetic run does not replace long physical
mouse play, all-car traversal or compositor-specific reproduction.

The distant lake bank now has broad longitudinal relief rather than a flat crest. The extra
height is bounded to 38 m, begins beyond 455 m waterward, and fades over 400 m at both ends of
the authored lake section. It reuses the existing terrain grid and adds no triangles or draw
batches. Far-bank trees now use the rendered terrain-triangle height instead of analytic height,
so changing the relief does not leave their roots floating. Tests cover bounds, continuity,
unaffected near/opposite shore and endpoint fade. The first low-amplitude version was visually
too subtle and was refined before acceptance. Four render/lakeside/waterfront/rural checks pass
(0.97 seconds), and the avatar-enabled 84 km capture passes the water-visibility check. Its
inspected skyline now has visible rises and dips; surface detail and natural tree silhouettes
remain unresolved. The README lake image was updated to this final capture.

Lakehurst's distant trees now form deterministic groves rather than a nearly uniform picket line.
Adjacent 50 m stripes share a 100 m group centre, with bounded longitudinal jitter, group-dependent
bank depth and hashed scale/species variation. The existing quality-specific counts are unchanged;
Medium and Low select stable subsets of High positions. Regression coverage checks forty stripes,
repeatability, bounds, minimum trunk spacing and substantial depth variation. Four render/lakeside/
High/material tests pass (2.91 seconds). The 84 km window capture was inspected: clumps and open
gaps replace the regular row, but the far bank remains too uniform in height and surface treatment
to be considered a finished landscape. This pass changes placement, not tree asset realism.

All five README showcase views were regenerated and visually inspected. This exposed an actual
landmark visibility bug: the Lakehurst water mesh existed, but the near-bank terrain ridge hid it
from the seated window camera. An initial suspicion that 82 km preceded the lake was incorrect;
route data defines its shore from 80.5 to 87.2 km. Moving the shot to the runtime-tested 84 km
position alone did not help. The lake-facing approach now stays below the railway-side sightline
and descends toward the basin, while the far bank and opposite terrain remain unchanged. The
authoritative lake capture is now taken at 84 km and visibly contains water. A filename-specific
fixed-view image check requires a substantial teal water band below the skyline; the old capture
failed with only 68 matching pixels and the corrected capture passed. This is a scene-specific
occlusion regression, not a general water-recognition algorithm.

Lowering the near bank also exposed misplaced facade attachments: house/townhouse/apartment
front windows and some doors used route elevation instead of their building's terrain-adjusted
base. They now inherit the building base, matching the already-correct end-wall windows and
apartment entrance. The final avatar-enabled lake capture was inspected with the floating-window
artifact removed. Four render/lakeside/waterfront/urban targets pass (1.00 second); showcase
validation was run again after replacing the lake image. World layout still needs authored
bank landscaping, a less repetitive far-shore tree line and better road/shore integration.

Pitched roofs now include a six-strip rounded ridge cap and slim continuous metal eave gutters,
using their existing roof/world batches. Cap shoulders penetrate the roof by 20 mm while its
centre clears the ridge by 80 mm; the pitch-aware profile avoids coplanar overlap. Each roof adds
36 triangles without an extra draw. Geometry checks cover ridge height, shoulder overlap,
upward normals, exact budgets and zero-width rejection. Four render/High/Low/material checks
pass (7.32 seconds); the High 63.77 km capture was inspected and shows a restrained dark eave
edge on the distant pitched roofs. These are silhouette approximations, not hollow gutter
channels, individual ridge tiles or a complete drainage system; downpipes remain absent.

Pitched roofs on Medium/High now share a mipmapped project-generated tile material, with four
staggered 0.30 m columns and four 0.40 m courses per texture repeat. UVs use actual slope length,
so changing roof pitch does not stretch the tile size. One streamed opaque roof batch uses stock
CNA per-pixel lighting; Low retains vertex-colour roofs. The geometry audit also found both roof
slopes had downward normals and the gables faced inward. Winding is corrected, and two degenerate
gable triangles per roof were replaced by proper triangles. Unit coverage checks upward roof
normals, physical UV extent, nonzero gable area, texture determinism/contrast and opaque mip
reduction. Four render/High/Low/material targets pass (7.91 seconds). The actual High 63.77 km
capture was inspected: pitched roofs are visible and the material is subtle at that distance.
The attempted 52 km left-window baseline contained no roofs, so it is not a before/after proof.
Tile geometry, weathering, ridge caps and gutters remain future improvements; no photorealistic
roof result is claimed.

A 900-frame High/no-avatar/no-audio 60x integration run of the updated shadows completed from
52 to 173.287 km, generated 492 chunks, retained seven and rebuilt the streamed world 482 times
with 1,687 buffer reuses. World-generation total/max were 8,781.6/36.3 ms; steady frame mean/p95
were 66.8/102.3 ms on the test renderer. The final countryside capture was inspected. This is
accelerated streaming evidence, not a physical-input test, memory-leak certification or a 60 FPS
result. Afterward, shadow sectors were changed to share radial surface samples: six-sided hulls
now require 61 rather than 90 receiver lookups, without reducing geometry. Tests assert exact
callback budgets alongside area and normal checks. Four focused render/urban/High/material tests
pass (2.93 seconds), and the matched 63.728 km before/after PNG comparison reports zero differing
pixels. No end-to-end FPS improvement is claimed without a matched performance measurement.

House/townhouse shadow dimensions now come from the same residential envelope used to build
their walls and roofs. House projections include the 8.15 m ridge envelope and 7.7 by 10.8 m roof
overhang instead of a 6 m height estimate; townhouse projections include their three wall-height
variants and 0.44 m roof cap instead of a fixed 8.4 m estimate. All dimensions follow feature scale.
Tests cover three scales and repeated variant cycles; five render/urban/rural/High/material checks
pass (2.96 seconds). The matched High 63.728 km capture was inspected and shows the longer
townhouse silhouette. Roof bounding boxes still approximate pitched-roof shape, omit chimneys,
and do not introduce general scene shadow receivers. Existing draw and tessellation policies
are unchanged.

Ground-projected house/townhouse silhouettes now use the convex hull of the footprint and its
sun-projected copy rather than a fixed six-corner outline tied to one light quadrant. A bounded
four-way subdivision samples the rendered terrain triangles and lifts samples over the building's
own raised residential plot. This replaces the old analytic-height receiver that could bury parts
of the shadow. Regression coverage checks nine shift combinations, including vertical light,
exact projected area, upward normals and sampled sloped-ground height; the shadow is bounded to
96 triangles per building. The render unit target and four urban/rural/High/material runtime
targets pass (0.03 and 2.99 seconds). The matched High 63.728 km capture was inspected and shows
the silhouette beside the middle building. The matched view rises from 732,856 to 742,936
triangles while retaining 104 draws. These remain opaque-building ground approximations:
roof height is approximate, plot boundaries are sampled rather than exactly clipped, and other
buildings/vehicles/roads are not general receiving surfaces. This is not full shadow mapping.

Residential contact-shadow bands were being hidden by their own raised plot surfaces: the
transparent bands sat 26/29 mm above the building base, but opaque plots reached 64 mm. Both
bands now use the plot's shared thickness constant as their receiving-surface base before adding
their existing clearance. Non-residential features retain their previous placement. No geometry,
draw batch or transparency policy was added. Four render/urban/High/material tests pass
(3.87 seconds); the matched High 63.728 km capture was inspected. The visible result is a subtle
base-contact correction, not directional shadow mapping. Projected building silhouettes still
use an approximate terrain receiver and do not shadow other buildings or road vehicles; that
larger lighting limitation remains open.

The detached horizontal strips below selected townhouse windows were traced to an unrelated
fixed height of `3.75 * scale`, rather than the actual second-floor sill underside. They are now
small facade-mounted planters, placed from the window centre, half pane height and sill thickness.
Their back overlaps the facade on either route side, and their top meets the sill; dark soil and
restrained green fill distinguish them from floating architectural trim. The existing deterministic
selection is preserved. Each replacement adds 24 triangles to the same batch. A geometry test
checks both mirrored sides, sill attachment and footprint bounds. Four focused render/urban/High/
material tests pass (2.91 seconds). The matched 63.728 km capture was inspected and shows the
previous visible gap closed; detailed foliage inside these small planters remains approximate.

Nearby facade/end-wall windows on Medium/High now use four raised frame rails rather than a
solid backing box hidden behind a thicker glass box. Previously the 135 mm pane protruded beyond
the 105 mm frame; the 220 mm open frame now leaves a 42.5 mm reveal on either face. Existing
opaque glazing, sill and mullions remain; this does not cut physical openings into building walls.
Low and distance-collapsed windows retain their cheaper path. Each detailed frame adds 36
triangles, with no new material or draw batch. A regression checks the open centre, pane clearance,
outer bounds, unit normals and invalid dimensions. Five render/urban/High/Low/material tests pass
(7.25 seconds). Matched High 63.728 km captures were inspected; the side-facing windows show
more pronounced frame depth. Broad flat facades, repeated layouts and detached-looking facade
details remain visible and still need further architectural work.

The complete current 56-target CTest suite passed in one run (98.39 seconds, two concurrent
tests) after the foliage material integration. This includes three new actual OpenGL launches
at the same 68.49 km view: Low must report zero `foliage_material_triangles`, while Medium and
High must report a nonzero drawn material batch. The geometry regression additionally requires
identical UVs after an origin translation. Quality changes invalidate the world anchor in the
existing renderer setter; live menu switching is not claimed as newly tested by these separate
launches. The full suite also covers interactions, station exchanges in both directions, terminal
reversal, announcements, avatar population, streaming views and install layout. Passing these
automated checks does not close the physical-input freeze or establish visual realism.

Near broadleaf crowns now have a separate opaque, mipmapped foliage material on Medium/High,
using a project-generated 128-square periodic leaf pattern and stock CNA per-pixel lighting.
One streamed batch replaces the corresponding vertices in the flat-colour world batch; the
matched Medium 68.49 km view retains 530,856 total triangles and increases draws from 86 to 87.
The before/after captures were inspected: fine leaf structure is visible instead of a uniformly
smooth surface. This remains an approximation, with closed lobe silhouettes and repeated leaf
shapes rather than photorealistic leaf clusters. No performance improvement is claimed.
An initial atan2-based UV implementation failed the seam-span regression; it was replaced with
topology-derived longitude coordinates and per-triangle pole midpoints. All five focused
render/rural/High/Low/material-capture tests then passed (7.20 seconds), including deterministic
opaque texture generation, mip reduction and bounded UV spans. Dependencies remain unchanged.
A 240-frame High/no-avatar 30x streaming run then reached 63.728 km from 52 km, with 46 world
rebuilds, 170 buffer reuses and seven retained chunks. It exited normally and its final capture
was inspected. The short steady-frame sample averaged 51.3 ms on the test renderer; this checks
material streaming integration, not a 60 FPS claim or a long-term memory-stability certification.

Medium near broadleaf trees now use a reduced clustered crown instead of five large overlapping
ellipsoids. Nine six-sided lobes in three interleaved tiers expose the existing branch structure
and give the silhouette smaller-scale variation; High retains fifteen seven-sided lobes and Low
is unchanged. The Medium crown costs 324 triangles versus the former 270, without a new draw
batch or material. Wider reduced-detail lobes prevent a hollow ring while keeping the same tested
corridor envelope. Determinism, bounds and exact geometry budgets are checked for four seeds at
both detail levels. Three focused tests pass (0.93 seconds), and the actual Medium 68.49 km
capture was inspected against the previous identical view. Branches and individual leaf masses
are clearer, but the result remains stylized: smooth opaque lobes do not yet provide convincing
leaf-edge breakup or fine foliage shading. No frame-rate improvement is claimed.

Medium/High distant broadleaf crowns now retain smooth normals and object-local canopy shading
instead of switching to flat per-face bands beyond 85 metres. The existing six-sided distant
topology, triangle counts, draw batches and Low profile remain unchanged. Foliage regression
coverage now exercises both six- and nine-sided lobes, preserving positions/indices, normalized
normals and colour stability after origin translation. Three focused render/rural/material tests
passed (0.97 seconds). Matched Medium 68.49 km captures were inspected before and after: this is
a subtle distant-shading correction, not a replacement of the still visibly polygonal near-tree
silhouette. More realistic vegetation remains an open art-direction item.

The extended private-X11 probe completed twelve phases with Medium-quality avatars enabled.
Its 47 five-second trace records span approximately four minutes: 1,581 nonzero look samples,
28 trace intervals with captured motion progress, thirteen capture acquisitions and exactly four
deliberate focus losses. Pause, map and focus-target cycles repeated four times each; final state
was unpaused, map closed, standing after one stand transition, at approximately (0.4,11.7).
All harness final-state predicates were independently rechecked against the log, preserved as
`build/x11-input-test/extended-probe-passed.log`. Phase 8 and
11 captures were inspected and show different actual rendered views with scenery progressing.
The game completed 3,004 measured frame intervals, retained seven chunks and thirteen ready
avatar visuals. Recent 2,048-frame mean/p95/p99 were 79.5/125.5/133.9 ms, with a 153.4 ms window
maximum and 1,061.1 ms all-run maximum. These private-display timings are not a hardware desktop
benchmark, but they are not acceptable evidence of a 60 FPS experience either. No camera freeze
was reproduced in this run; the user-reported long-play freeze remains open. This is neither a
physical mouse test nor a complete six-car walkthrough. Audio was disabled. Reproduce with
`INPUT_PROBE_PHASES=12 INPUT_PROBE_AVATARS=1 bash tools/test_x11_input.sh`.
After adding strict option validation and a persistent exit-status receipt, the default
three-phase/no-avatar probe was rerun successfully: exit status zero, including its deliberate
focus-loss cycle. Invalid phase input `08` is rejected with status two before launching Xvfb.

Input validation is no longer limited to offscreen controller tests. An isolated Xvfb display
was created outside the sandbox and the actual X11 game window received synthetic mouse/key
events; the user's desktop was not targeted. New opt-in five-second `INPUT_TRACE` records showed
continued nonzero motion and changing yaw across pause and map release/reacquire cycles.
The roughly 50-second trace reached 460 motion samples, and final state reported three capture
acquisitions, zero unexpected losses, one stand transition and movement from (1.04,11.275) to
approximately (-0.2,12.5). The before/phase-0/phase-2 window captures were visually inspected.
Evidence is in `build/x11-input-test/`; reusable harness: `tools/test_x11_input.sh`.

The first harness attempt used very short taps, failed to capture the mouse and eventually
timed out; its log is preserved as `short-tap-failed.log`. That is not a successful camera test
or proof of a dependency bug. Held 180–250 ms keys/clicks plus an explicit capture precondition
made the subsequent probe valid. The final harness also checks final acquisition/standing state.
This test used Medium quality, no avatars and X11—not physical hardware or the user's normal
compositor—and is too short to close the reported intermittent freeze. No input-behaviour fix
is claimed here: the changes add reproducible validation and diagnostic evidence.

A fixed 128-entry, per-world-build route-point cache now avoids recomputing relative route
geometry and heading trigonometry for the many lateral samples at the same longitudinal point.
Keys include exact distance, reference and route identity; there is no coordinate rounding or
cross-frame cache lifetime. Regression checks cover 1500 samples / 6000 queries, collisions and
both extended endpoints, requiring exact equality of the final float coordinates with the
uncached path. Eight focused tests pass (17.29 seconds), including station exchange and reversal.

A 600-frame High/no-avatar 30x run from 52 km reached 85.039 km with seven chunks. It recorded
1527674 cache hits and 803143 misses (about 65.5% avoided calculations), 132 rebuilds and 474
buffer reuses. Generation total/max was 2501.3/23.2 ms, upload submission 1874.2/24.9 ms and
outer rebuild 4376.1/44.7 ms. The 419 steady intervals had mean 47.0 ms, p95 80.4, p99 86.1,
maximum 91.8; wall time 30.10 s, peak RSS 534504 KiB. Final rendering was inspected in
`/tmp/route-point-cache.png`; log: `build/route-point-cache.log`. Although faster than the
preceding buffer-reuse sample, different endpoints/workloads and shared-machine load prevent
attributing the whole timing difference to this cache. Avoided calculations and equivalent
coordinates are verified; full-window synchronous rebuilding remains an architectural limitation.

Streamed world meshes now reuse exact-sized CNA dynamic vertex/index buffers independently,
performing complete Discard uploads and reallocating only when that buffer's element count
changes. Static train/prop buffers are unchanged. This deliberately avoids excess-capacity
partial uploads, whose documented CNA fallback merges and uploads the untouched suffix too.
The runtime `world_buffer_reuses` counter counts retained CNA objects, not guaranteed reuse of
the driver's backing allocation. Terminal-reversal regression now requires actual reuse.

Eight initial focused tests pass (14.79 seconds). A subsequent 600-frame High/no-avatar 30x run
from 52 km reached 98.065 km, retained seven chunks and reported 184 world rebuilds with 642
buffer reuses. Generation total/max was 5282.5/40.2 ms, upload submission 3774.6/39.4 ms, outer
rebuild 9058.0/70.4 ms. Its 419 steady intervals had mean 75.3, p95 121.6, p99 136.5 and max
165.7 ms. Wall time was 45.63 s and peak RSS 640828 KiB. Final rendering was inspected in
`/tmp/world-buffer-reuse.png`; log: `build/world-buffer-reuse.log`. This verifies operational
resource reuse, not an FPS gain: the older runs differ in scenery, endpoints and machine load.
Full-window CPU regeneration and significant frame delays remain unresolved.
The subsequent complete suite had 52 passes and one 30-second station-exchange timeout
(147.25 seconds total). That unchanged test passed alone in 8.79 seconds without increasing its
timeout; the earlier focused run also passed it. Other tests, including CPU-only simulation,
slowed during the full run, consistent with contention but not proof of its cause. Preserve
this as a timing-sensitive result, not an unqualified full-suite pass. The strengthened terminal
test passed with a nonzero buffer-reuse count. Dependency source worktrees remain clean.

A post-polish integrated application soak rendered 3600 High-quality frames with avatars and
dummy-device audio, beginning at 280 km with 120x service scaling. It exited successfully after
313.86 seconds (5 min 14 s), at 247.600 km, direction +1, dwelling. It reported 32124 simulated
seconds, 44 station legs, four terminal announcement events, 4433 generated chunks and seven
retained chunks. Population remained 38 with 18/18 avatar visuals ready; 14 audio instances
were active at exit. No warning/error/exception text appeared in the log. Final image:
`/tmp/realism-integration-soak.png`; log: `build/realism-integration-soak.log`.

This run strengthens operational evidence but exposes frame-pacing work, not a performance win.
The final 2048 post-warm-up intervals had mean 89.9 ms, median 89.0, p95 125.1, p99 145.2 and
maximum 186.0 ms. The all-run maximum was 2696.6 ms. There were 2465 synchronous world rebuilds:
92942.8 ms total, 90.8 ms max; generation 59676.2 ms (66.0 max), upload submission 33257.4 ms
(43.7 max). Roughly 30% of total wall time was spent inside world rebuilding under this deliberately
high transition rate. Peak RSS was 979044 KiB; no memory time series was recorded, so this does
not establish leak freedom. Shared software rendering and 120x scaling are not desktop 60 FPS
benchmarks, audible audio validation, physical mouse testing or a real-time endpoint journey.
Reducing synchronous full-window regeneration is therefore a higher-impact next reliability
task than additional small geometry embellishments. Periodic runtime telemetry is also needed
so future long runs expose progress and memory trends before exit.

Platform canopies now have a pale modular soffit with 30 mm panel joints, a centre seam and
two slim longitudinal support rails. Previously the large roof underside was simply the dark
bottom face of the roof box, dominating the upper part of platform-view captures. Downward-facing
panels are placed below the roof without replacing its weathering fascia/gutters, and the rails
leave the central light strip clear. The helper caps panel subdivision at 128 and remains in the
opaque world batch (a 30 m example adds 64 triangles). Bounds, downward normals, light-strip
clearance and empty-input checks pass with three other focused tests (5.51 seconds total).
The actual terminal capture `/tmp/platform-soffit.png` confirms readable panels and structural
depth where `/tmp/terminal-skyline-facades.png` showed a largely flat dark band. No new draw
batch or dynamic lighting was added; material detail and station-specific roof construction
remain opportunities for later refinement.

Terminal skyline blocks now have restrained window grids on their track-facing and two end
facades, plus a ground-floor entrance. These blocks are only 88–156 m from the track, so their
previous completely blank walls were plainly visible rather than hidden in distance haze.
The reusable helper emits two opaque quads per opening (frame and inset pane), with capped
floor/column counts, no new material or draw batch, and no glass sorting. Tests verify triangle
budget, both side orientations, outward normals and building bounds. Four focused tests pass
(2.76 seconds). `/tmp/terminal-skyline-facades.png` confirms readable facade rhythm in the actual
terminal view. These are still simplified background buildings; coherent surrounding streets,
surface variety and more authored station composition remain necessary.

Both terminal ends now retain 1200 m of textured terrain beyond the service endpoint instead
of a single flat, untextured apron box. Only the first/last chunk's terrain ribbon is extended;
ordinary chunks, streamed population and service distances are unchanged. Each loaded terminal
adds 1008 terrain triangles (24 longitudinal cells x 21 lateral bands x 2). Skyline blocks now
sample route-relative ground height and extend their walls 2 m below that base, avoiding exposed
foundations where broad terrain triangles slope. The former fixed-height apron was removed.
Range tests cover both endpoints and unchanged interior chunks. Five focused tests pass
(8.13 seconds), including terminal reversal. Actual High captures at station 1 and station 12
with reverse direction (`/tmp/terminal-terrain-start.png`, `/tmp/terminal-terrain-end.png`)
show terrain continuing under the terminal surroundings instead of the pale flat void. The
skyline remains intentionally coarse and the urban surroundings still lack convincing street
density; this grounds the existing composition rather than completing station realism.
After the sky, avatar-proportion and terminal-terrain changes, the complete 53-test suite passes
again (101.62 seconds). Both dependency worktrees remain clean. This does not replace the still
outstanding physical-mouse and long real-time journey validation.

Passengers now receive a project-local 0.82 uniform head-bone scale through CNA's documented
custom-pose draw API. The public description API has height/body-type getters but no editable
head-proportion control; no opaque description bytes or dependency sources were modified.
The adjustment is applied once to a fresh animation pose, after seated/walking/gaze layers,
for both onboard and platform figures. Native face/hair/headwear follow the head hierarchy.
An isolated test verifies preserved joint translation, uniform basis length, untouched other
bones and safe empty input. Render, High population and Low budget tests pass (23.66 seconds).
Actual 120-frame captures `/tmp/passenger-proportions.png` and `/tmp/platform-proportions.png`
show less oversized heads, attached necks/hair and a correctly attached native hat on the
inspected figures. These are randomized appearances, not matched-character A/B captures, and
do not validate every catalog accessory. Large eyes and stylized facial features remain;
this proportion adjustment does not turn CNA's catalog into realistic human models.

A fresh occupied-car capture found visible triangular cutouts along several rear headrest edges
(`/tmp/interior-current-audit.png`). The previously corrected passenger-facing cover was not the
cause: the rear shell began 68 mm from the crown centre while the crown extended 82 mm. The
rear assembly is now offset outward by 20 mm, giving at least 88 mm clearance without changing
the authoritative +/-480 mm seat collision bay. Regression checks cover both facing directions,
the entire crown envelope and the maximum seat envelope. Four focused render/interaction/material
tests pass (3.13 seconds). A second actual 120-frame High run with avatars,
`/tmp/seat-rear-crown-clearance.png`, confirms the rear-edge triangular intersections are gone.
Avatar appearances are randomized between these captures, so the comparison applies to the
unchanged seating geometry, not passenger styling. The issue was project geometry, not a CNA
defect. Avatar proportions and broader interior material realism remain separate open work.

Cloud texture generation now shades the rounded lobes using approximate surface depth and
upper-side illumination instead of an almost uniform white vertical gradient. Deterministic
low-frequency variation remains, and the existing soft alpha silhouette is unchanged. This
adds no textures, triangles, runtime animation or draw calls. The material regression additionally
requires at least 25 byte levels of luminance variation within dense cloud texels while retaining
determinism, transparent margins and dense centres. Four focused tests pass (3.18 seconds).
The actual window capture `/tmp/cloud-lobe-shading.png` shows readable internal lobe shading;
the repeated cloud silhouette and billboard approximation remain visible limitations. This is
a sunny-sky presentation refinement, not volumetric clouds or a dynamic weather system.

The sky is now a closed 960-triangle sphere instead of an open 256-triangle cylinder ending
500 m above its centre. The former top edge left higher viewing angles dependent on the flat
clear colour. A horizon-to-zenith gradient now covers every direction, with nondegenerate polar
triangles. Sky, sun and cloud rendering use rotation-only camera view matrices, removing nearby
parallax when walking between cars and keeping the sky within the 1100 m far plane. Outdoor
fog now uses the sky horizon colour for both material pipelines; the existing tunnel fog target
is preserved. This remains one sky draw, not an atmospheric scattering shader.
Geometry tests check both poles, radius, triangle area and colour ordering. Six focused render,
rural, material, tunnel and interior-sunlight tests pass (4.01 seconds). The actual High window
capture `/tmp/closed-daylight-sky.png` verifies the horizon/fog treatment; a physical mouse-driven
zenith inspection has not been performed. Clouds remain simple textured cards and the scene
still needs stronger material/vegetation fidelity before it can be described as realistic.

Hedge profiles now vary their width, centreline and crown height within the original footprint.
Vertex colours darken the lower growth and introduce restrained seeded variation across the
crown. Shared section edges carry identical positions and colours, avoiding colour seams as
well as geometric cracks. Tessellation, batch count and opacity are unchanged. New checks
cover deterministic output, footprint bounds, colour variation and shared-edge colour equality;
four focused render/rural/material tests pass (2.79 seconds). The actual High 52 km capture
`/tmp/hedge-natural-profile.png` shows the less uniform outline, but also confirms that broad
facets remain readable. This is a bounded silhouette/material-colour refinement, not leaf-level
vegetation or a completed realism pass.

The 500 m rural parcel boundaries are now continuous 96 m terrain-following hedgerows rather
than ten separated, level pieces. Their approximately metre-high crown follows the rendered
terrain, with the base buried by 8 cm. The cross-section is sampled once per ring and shared
exactly at each adjacent section; only the two outer ends have caps. Each boundary uses 648
triangles versus 780 for the previous ten 9.15 m pieces. The diagnostic boundary counter retains
its nominal 9.6 m span units. Authored crop-field edges retain their existing segmented layout.
The terrain-cell sampler is shared with vegetation shadows and now derives cell origins from
the actual chunk length, including truncated terminal cells, removing the earlier alignment
assumption. New tests verify slope-relative bounds, exact shared edges, sample count and mesh
budget. Six focused render/rural/material/bridge/terminal tests pass (6.30 seconds).
The actual High 52 km capture `/tmp/continuous-field-hedges.png` confirms that the regular gaps
and stepped bases are gone. The hedge is still an opaque faceted mass without leaf detail;
continuous geometry alone does not make this landscape photorealistic.

Tree and bush contact shadows now have a seeded twelve-sided outline and an alpha-feathered
outer band instead of rectangular patches. Nearby trees also receive a ground-only canopy
projection following the existing sunny light direction. The obsolete opaque green base box
was removed. These share the existing transparent ground-shadow batch; each footprint costs
36 triangles, so this is additional geometry rather than a performance optimization.

The first actual 52 km capture exposed a placement error: sampling the analytical terrain
height can bury a decal below the rendered, coarser terrain ribbon. Vegetation shadow vertices
now interpolate the ribbon's a-c triangle diagonal, with a small depth separation and a cached
four-corner cell per footprint. Tests cover both triangle halves, the diagonal, upward winding,
footprint bounds and transparent outer vertices. Five focused tests pass (5.97 seconds).
`/tmp/vegetation-shadows-surface.png` shows restrained ground contact without the previous dark
rectangular pads. The projected shadows remain subtle from the seated window view; this is
not shadow mapping, does not occlude other objects, and does not complete vegetation realism.
At this stage the terrain lookup assumed 250 m chunks / 50 m terrain cells; the subsequent
continuous-hedge work above replaces that assumption with chunk-relative lookup. Other scenery decals
still use their older height placement. Physical mouse freezing remains an independent open
issue; these offscreen render checks are not manual input validation.
The complete current 53-test CTest suite also passes (112.63 seconds), including avatar
population, audio, station exchange, terminal reversal and installation layout checks.

Field hedges now use a six-point clipped/rounded cross-section with seeded longitudinal height
variation rather than opaque boxes. Both the 500 m parcel boundaries and authored crop-field
edges use the same bounded helper, retaining their route-relative placement and terrain bases.
It uses opaque geometry in the existing world batch, with no additional materials or transparency.
Nine metres of hedge uses 68 triangles instead of 12; the helper caps subdivision at 128 spans.
Tests check exact tessellation, footprint/height bounds, outward side/upward roof normals and
empty input. Four focused render/rural/material tests pass (3.44 seconds). The High 52 km
capture `/tmp/hedge-profile.png` shows softer upper edges compared with the previous block
hedges, but the result is still a simple trimmed hedge mass with regular segment gaps, not
leaf-resolved vegetation. This is a modest silhouette improvement, not a completed landscape pass.

A same-process route microbenchmark now resolves the previous ambiguous whole-game samples.
In the existing RelWithDebInfo build, six alternating-order pairs of 262144 identical inputs
gave full-metadata median 109.660 ms versus geometry-only 45.047 ms (2.434x ratio). Individual
geometry batches ranged 44.709–47.931 ms and baseline batches 107.762–117.583 ms. All 1024
distinct inputs, including endpoint extensions, matched coordinates within 1e-9; every timed
batch produced checksum -910813.345 at printed precision and passed the 1e-6 checksum comparison.
`tools/benchmark_route.cpp` is an optional, excluded-from-default CMake target, documented in
the architecture guide. This supports retaining the geometry-only optimization but does not
prove better frame pacing or FPS. It isolates the function cost and avoids different route
endpoints, resource uploads and software-renderer scheduling that confounded earlier runs.

World rebuild profiling now separates CPU geometry generation and CPU buffer creation/submission.
The pre-change 600-frame High/no-avatar 30x sample performed 128 rebuilds: 2441.9 ms generation
(26.6 ms max), 1426.3 ms upload submission (17.7 ms max), 3868.7 ms outer total. About 63% of
the measured total was generation; deferred GPU work is not included in the upload timer.
This led to a concrete hot-path correction: `Route::relativePose` previously called full
`sample()` twice, evaluating twelve station influence exponentials and unused environmental
metadata for every purely geometric placement. It now uses shared lateral/slope/elevation
functions directly. Full metadata sampling and endpoint tangent extrapolation remain unchanged.
New equivalence checks cover relative positions/headings in rural, station, tunnel and both
extended-terminal ranges. Seven focused tests, including terminal reversal, pass (8.49 seconds).
Baseline log: `build/world-stage-profile.log`.
The subsequent 600-frame sample reached 91.427 km with seven chunks and 157 rebuilds. Generation
totalled 3560.2 ms (47.5 ms max), upload submission 2416.5 ms (36.7 ms max), and the outer total
5977.4 ms. Post-warm-up mean/p95/max were 59.0/96.9/146.5 ms. This is slower, including the
unchanged upload stage, so these sequential different-endpoint runs do not establish a speedup.
`build/world-stage-geometry-only.log` records the result. Correctness and removal of unnecessary
metadata work are proven independently; a controlled same-process geometry benchmark or stable
hardware replay is needed to quantify its performance benefit. Do not treat these changes as
resolution of the stream-boundary hitch or the physical input-freeze report.

Ellipsoid construction now precomputes longitude sine/cosine pairs once and latitude pairs once
per ring instead of recomputing them at every face corner. Typical scene tessellation uses a
fixed stack table; a dynamically sized fallback preserves support for more than 64 slices.
This does not add retained mesh caches or change crown detail. A reference-generator regression
compares positions, normals, colours and indices for 3, 7, 16 and 65 slices; five focused render,
rural, material and toilet tests pass (2.74 seconds). This reduces repeated CPU arithmetic in
the many crown lobes and other ellipsoid fixtures, not full-window upload cost or draw calls.
A following 600-frame High/no-avatar 30x run reached 86.622 km with seven chunks and 138 rebuilds
(4426.1 ms total, 47.6 ms max). The 419 post-warm-up intervals had mean 50.4 ms, p95 86.9 ms,
p99 95.1 ms and max 104.6 ms; wall time was 30.82 s and peak RSS 526016 KiB. This sample was
slower than the preceding staging-reuse sample, so no measured speedup is claimed. Different
endpoints/rebuild counts and shared-machine timing prevent causal attribution; the algorithmic
arithmetic reduction does not establish an end-to-end gain. Log: `build/frame-timing-cached-angles.log`.
The larger synchronous full-window regeneration remains the next performance target.

The largest opaque-world CPU staging buffer is now reused across rebuilds instead of allocating,
growing and freeing fresh vertex/index vectors at every chunk boundary. A regression verifies
that `clear()` preserves capacity, removes all old geometry and restarts indexing at zero.
GPU upload ownership and generated geometry are unchanged. Seven focused tests pass (3.28 s).
Runtime reporting additionally separates intervals after 180 drawn frames and avatar readiness;
both endpoints must qualify, and ordinary chunk/station transitions remain included.

A 600-frame High, no-avatar, 30x run from 52 km reached 82.755 km with seven loaded chunks and
123 rebuilds: 3675.2 ms total, 44.8 ms maximum. Overall 599 frame intervals had mean 46.3 ms,
p95 76.5 ms, p99 82.6 ms and max 1364.5 ms. The 419 post-warm-up intervals had mean 43.6 ms,
p95 74.9 ms, p99 78.8 ms and max 86.2 ms. Wall duration was 27.93 s and peak RSS 518572 KiB
(`build/frame-timing-staging-reuse.log`). Thus the >1 s stall in this sample occurred during
warm-up, while meaningful steady-state hitches remain. Earlier sequential runs took different
routes over their fixed frame budgets; this is not a controlled percentage-speedup claim.
Persistent staging retains one high-water capacity, so its memory tradeoff must also be monitored
through longer terminal/city loops. Full-window geometry generation and upload remain synchronous.

Frame pacing now has a bounded 2048-sample draw-to-draw wall-time ring. The final state report
includes sample counts, mean, nearest-rank p50/p95/p99, window maximum and lifetime maximum.
Recording has no allocations; sorting occurs only for the final report. Unit coverage exercises
empty input, invalid samples, percentile ranks and eviction while preserving the lifetime peak.
Five targeted core/render/rural/material tests pass (3.31 seconds). A 600-frame High offscreen
30x run from 52 km with avatars enabled produced 599 intervals: mean 73.6 ms, p50 59.7 ms,
p95 117.0 ms, p99 149.3 ms, maximum 2401.9 ms. It reached 97.221 km across two station legs,
kept seven chunks and prepared all 18 avatar visuals. Its 180 world rebuilds totalled 7774.4 ms
with a 78.7 ms maximum. Log: `build/frame-timing-canopy.log`. Startup is included; these metrics
do not attribute the 2.4-second gap to a specific subsystem, and machine scheduling can contribute.
They also do not reproduce or resolve the physical mouse capture freeze. Steady-state and
subsystem-specific timing remain necessary before claiming hitch-free gameplay.
The following same-options 600-frame run with `--no-avatars` still reported a 1378.1 ms maximum:
mean 75.5 ms, p50 64.6 ms, p95 129.9 ms and p99 160.0 ms. It ended at 99.730 km, with 190
rebuilds totalling 8555.4 ms (72.2 ms maximum). `build/frame-timing-no-avatars.log` therefore
contradicts attributing every long interval solely to avatar work. Sequential offscreen runs
with different resulting route endpoints and startup included are diagnostic, not a controlled
GPU benchmark; next profiling must separate startup and stage-specific steady-state costs.

Post-canopy integration soak: a 1200-frame High/OpenGL33 offscreen run, avatars enabled,
audio disabled, 30x time and development dwells travelled 64.592 km (52.000 to 116.592 km)
across three station legs. It generated 265 chunks while retaining seven, with all 18 avatar
visuals ready and 38 passenger records at exit. The process exited normally in 63.82 seconds;
peak RSS was 860236 KiB. Its 258 synchronous world rebuilds consumed 9265.4 ms total (35.9 ms
mean), with a 60.1 ms maximum. Final-window FPS was 19.7, not a whole-run average. The final
capture `/tmp/canopy-streaming-soak.png` and log `build/canopy-streaming-soak.log` are retained
outside source control. This exercises repeated streaming and station progression, not manual
input, full-route reversal, real-time audio or leak freedom. It is not directly comparable to
earlier soaks with different frame counts/endpoints. The measured rebuild spikes remain an
important performance issue before further scene-density increases; this run predates the
frame-time percentile telemetry described above.

High-quality near broadleaf trees now use fifteen smaller overlapping crown clusters distributed
across three staggered tiers instead of five large ellipsoids. A route-distance/variant seed
changes orientation and small height/size offsets reproducibly. The existing trunk/branch and
shadow batches are retained, and all points remain inside the old maintained-corridor envelope.
Unit tests cover deterministic geometry/colours, four seeds and exact bounded tessellation.
Each new crown has 630 triangles versus the prior 270; Medium/Low and distant trees keep the
cheaper shapes. Four targeted tests pass on the final warning-clean build (2.74 seconds).
The matched High 52 km image `/tmp/foliage-clustered.png` has a visibly broken outer contour
compared with `/tmp/foliage-after.png`; the scene reports 590010 triangles and 87 approximate
draw calls. The result remains clustered/stylized rather than leaf-resolved vegetation, and
moving-view performance with the extra canopy geometry still requires measurement.

Near broadleaf crown lobes on Medium/High now use smooth ellipsoid normals and continuous
object-local vertex colour variation with darker undersides. This replaces face-by-face colour
bands without adding vertices, triangles, material switches or alpha sorting. Low and distant
trees retain their previous shading. Regression checks preserve indices/positions, Low colours,
unit normals and colour stability across a 128 m origin translation (one quantization level
tolerance). Four focused render/rural/material tests pass (4.18 seconds). Matched High 52 km
window captures `/tmp/foliage-before.png` and `/tmp/foliage-after.png` confirm smoother lighting
on the large foreground broadleaf trees. Their polygonal outer contour and merged-lobe shape
remain obvious: this is a lighting correction, not realistic foliage. A more broken silhouette
and leaf-scale structure are still required before the vegetation realism item can close.

An occupied-seat inspection after the shaped-fabric change prepared all 18 CNA visuals and
captured two seated passengers at close range (`/tmp/occupied-shaped-seats.png`, 120 frames,
High, 238 km). It exposed a stretched skirt hem ahead of one passenger's knees, not a visible
padding intersection. The back-contact region is partly hidden, so the image is not proof that
every outfit fits. The probable catalog/custom-pose incompatibility is recorded in `bugs.md`.
Civilian appearance scoring now strongly prefers non-skirt candidates from the inspected CNA v3
catalog, using at most 32 untouched public random descriptions. Format/catalog guards and both
known skirt IDs have regression coverage. This is a bounded, best-effort workaround that reduces
wardrobe variety, not a repair to CNA or a general garment simulation solution.
After the workaround all 53 CTest targets pass (91.01 seconds). A fresh 120-frame close capture
(`/tmp/occupied-seat-wardrobe.png`) prepared all 18 visuals and shows the two foreground seated
passengers in trousers/shorts without the stretched hem. Random appearances differ between runs,
so this is observed sample coverage, not an exhaustive wardrobe-fit guarantee. Both dependency
worktrees remain clean. The unresolved physical mouse-camera freeze is unaffected by this work.

Backrest woven inserts now have a bounded 18 mm padded profile instead of a single flat quad.
A 4-by-6 patch uses analytic smooth normals and a squared-sine displacement that returns to zero
with a flat tangent at the perimeter. The shared mesh builder now supports smooth textured quads;
tests check both orientations, clockwise winding, unit/outward normals, maximum depth, edge
continuity and unchanged 0.32 m UV scale. Six focused tests pass (4.82 seconds). High captures
`/tmp/seat-shaped-fabric.png` and `/tmp/seat-shaped-fabric-close.png` show intact inserts without
visible cracks, but the effect is subtle under current cabin lighting. This is not a finished
ergonomic seat model. The material mesh grows from 752 to 9124 triangles across all 182 seats;
the representative car-three scene still reports 87 submissions. No extra texture or material
pass was added. Avatar contact against the new padding needs subsequent occupied-seat inspection.

Front upholstery piping was visually oversized: vertical strips were 18 mm wide, the horizontal
strip 30 mm and cushion edging up to 24 mm. All are now restrained 4 mm seams with lower-contrast
car-family colours. No triangle, material or submission counts change. Four focused render,
interior and material tests pass (3.75 seconds). The matched High car-three capture
`/tmp/seat-piping.png`, compared with `/tmp/seat-head-covers.png`, removes the conspicuous pale
rectangular outlines while retaining woven cloth and readable edge stitching. This improves
textile scale only; the broad backrest is still too planar and needs shaped upholstery rather
than more surface lines. Neither this image check nor the targeted tests validate physical input.

The dark triangular marks at the top of every front headrest cover were geometry intersections:
the cushion crown extended 82 mm from its centre, while the old cover sat only 76 mm away.
The replaceable cover now occupies 85–88 mm on the passenger-facing side, with rounded corners
and a thin solid edge. Tests prove every cover vertex clears the crown envelope in both seat
directions. Crown normals are now smooth without changing its tessellation. Five focused tests
pass (3.44 seconds); matched car-three High captures `/tmp/seat-rear-shells.png` (before) and
`/tmp/seat-head-covers.png` (after) show the triangular intrusions removed and softer cover edges.
No dependency changes or new material submissions were required. The cover still has a simple
flat colour rather than a convincing textile finish; the larger upholstery-shape task remains open.

Rear seat shells now have a shallow rounded solid skin rather than a rectangular overlay.
The literature pocket has rounded corners, a projecting lip and a dark mouth approximation;
the upper hand recess is also rounded. These remain in the opaque interior batch with no new
draw calls. Bounds tests cover both seat directions, unit normals, and the maximum 35 mm rear
offset from the existing rear surface. Five focused core/render/interior/material tests pass
(5.01 seconds). High offscreen captures `/tmp/seat-rear-shells.png` and
`/tmp/seat-rear-shells-aisle.png` show the rear pockets between foreground rows, without obvious
overlap. They do not provide a close oblique inspection of every shell, and the front upholstery
still reads too rectangular. Recesses are layered colour/geometry approximations, not hollow
functional handles. The representative car-three view reports 545602 triangles and 87 approximate
draw calls; rounded solid details add geometry and need to remain bounded as refinement proceeds.

Matched daytime captures identified the conspicuous dark band over the windows as primarily the
underside of the continuous 70 mm luggage shelf, rather than the upper-wall material. The rack
now uses four 30 mm aluminium slats separated by 25 mm gaps and a rounded aisle retaining lip.
The same opaque interior batch handles all geometry; no transparency, new texture or draw is
required. Existing toilet exclusions are preserved. Crossbars now align with alternate window
mullions and connect to wall mounting plates instead of ending short of the wall; door-adjacent
mounts are skipped. Luggage bases meet the deck at 2.23 m. Geometry tests cover both mirrored
racks, their bounds and empty spans. The initial seven focused tests passed in 5.05 seconds;
`/tmp/luggage-rack-day.png` confirms the lighter open silhouette against the preceding
`/tmp/ceiling-cove-day.png`. This correction does not change collision or passenger simulation.
After the mounting correction the same seven tests passed again (4.01 seconds), and
`/tmp/luggage-rack-mounted.png` confirms the final mullion attachment in the running renderer.

The six saloons now use quarter-round upper-wall coving instead of rectangular corner bars.
Each 250 mm radius skin connects the wall at 2.664 m to the ceiling at 2.914 m, using eight
opaque segments and the existing polymer-grain material batch. Twelve skins add 192 material
triangles while replacing 144 box triangles: 48 net triangles, no extra submissions or textures.
Both sides have tested inward normals and bounded dimensions. Six focused render, material,
interior, vestibule and tunnel tests pass (4.11 seconds). The actual High daytime capture
`/tmp/ceiling-cove-day.png` shows the rounded upper transition and clear luggage shelf space.
This is a cross-section improvement, not a claim of a realistic finished cabin: the large dark
upper-wall band and flat seat-back trim remain conspicuous in the same image.

Onboard avatar submission now uses conservative sphere/frustum rejection against the current
view/projection matrix. A 1.5 m radius around floor height + 1 m includes seated/standing poses
and handheld props. The test rejects only side planes and wholly behind-camera bounds, not
near/far depth planes. Assignment, appearance identity, animation, gaze, gait and seat-transition
updates still run for culled characters; GPU priming remains unconditional. Platform people and
contact shadows are unchanged. `culled_onboard_avatars` is a separate per-frame diagnostic;
legacy `visible_npc` still counts nearby assigned characters, not strictly submitted characters.
Regression coverage includes translated/rotated cameras, behind-camera bounds, frame-edge
intersections and near-plane overlap. This does not address the unresolved physical mouse freeze.
The complete 53-target suite passes in 96.70 seconds. A repeated 300-frame High window-view
profile at 238 km reports 19.7 end FPS (previous sample 15.7), 16.44 wall seconds (20.54),
477572 KiB peak RSS (477972), one 34.2 ms world rebuild, all 18 avatar visuals ready, and 11
nearby onboard avatars conservatively rejected in the final outward-looking frame. These are
single offscreen samples including startup, not a guaranteed desktop speedup. The Low saloon
capture still shows foreground seated passengers and the aisle pedestrian. Logs:
`build/profile-avatars-culled.log` and the CTest output in `build/Testing/Temporary/`.
A separate 120-frame High passenger-view capture (`/tmp/avatar-culling-saloon.png`) shows both
foreground seated avatars and distant passengers, with 18 visuals ready, 11 assigned and four
culled. It also confirms remaining art gaps: toy-like native avatar proportions, faceted tree
silhouettes and flat distant terrain. This was visual offscreen inspection, not manual mouse play.

World rebuild profiling now reports cumulative count, total milliseconds and maximum milliseconds
using a monotonic clock around static-world generation/upload. Three sequential 300-frame High
offscreen runs started at 238 km, window view, audio disabled:

| Mode | End FPS | World rebuilds | Total / maximum rebuild ms | Wall seconds | Peak RSS KiB |
|---|---:|---:|---:|---:|---:|
| 0.01x, no avatars | 26.4 | 1 | 31.2 / 31.2 | 13.53 | 323576 |
| 30x, no avatars | 21.5 | 50 | 1725.6 / 55.9 | 15.77 | 499280 |
| 0.01x, avatars | 15.7 | 1 | 46.3 / 46.3 | 20.54 | 477972 |

The moving run travelled 12.52 km and kept seven chunks; the avatar run finished with all eighteen
visuals ready. These are diagnostic samples, not controlled GPU benchmarks: final FPS is only the
last reporting interval, the moving endpoint differs, and wall time includes initialization.
Still, they identify two concrete next investigations: synchronous full-window rebuild spikes
and avatar submission cost when looking out of the train. At that measurement, onboard selection
was distance-based (28 m), without view-frustum rejection. The subsequent culling change above
preserves this selection and its identity/animation continuity.
Four focused render/rural/urban/tunnel targets pass (2.19 seconds). Logs are ignored
`build/profile-stationary.log`, `build/profile-moving.log`, and `build/profile-avatars.log`.

Post-vegetation streaming soak: a 2,400-frame High/OpenGL33 offscreen run with avatars enabled,
audio disabled, 30x time and development dwells travelled from 52.000 to 238.015 km. It completed
seven station legs, generated 751 chunks and ended with seven loaded chunks, 38 passenger records
and all 18 avatar visuals ready. The run exited successfully after 175.09 seconds; `/usr/bin/time`
reported 958,116 KiB peak RSS, no swaps and no major page faults. Final telemetry reported 112
draw calls, 504,694 triangles and 13.9 FPS. The saved final window capture was inspected.
This is a bounded accelerated streaming check, not a full service reversal or manual-input test.
Peak RSS alone does not establish leak freedom, and the final FPS is not a whole-run percentile
or a physical-GPU benchmark. The geometry/streaming cost now needs a comparable stationary versus
moving profile before further scene-density increases. The initial launch without an explicit
video driver failed immediately because no display was available; the completed run explicitly
used SDL's offscreen driver. Evidence log: ignored `build/vegetation-soak.log`.

Distant-ridge correction: the old `(chunk*5 + side*3 + layer*7)%5` discarded the entire chunk
contribution, repeating identical hill parameters along each side/layer. A deterministic 64-bit
mix now selects the variant; a regression requires all five variants across a representative
chunk range for each side/layer and checks repeatability. Medium/High use finer ellipsoid profiles
and analytic smooth normals instead of the eight-sided, three-stack faceted mass; Low retains
that geometry budget. Matched 52 km High captures replace the slab-like skyline with rounded,
varied hills. Five focused render/rural/urban/lakeside/High-MSAA targets pass (2.84 seconds).
Hill counts and submissions are unchanged; Medium/High vertex counts increase. These remain
overlapping procedural hill masses, not a fully authored continuous distant terrain system.

Foreground grass correction: GrassPatch features and drain-bank tufts now use tapered bent,
double-sided opaque ribbons rather than 30 cm-deep upright boxes. Blade heights/yaws vary within
the existing placements; streaming, feature counts and quality-tier selection are unchanged.
Six triangles per blade replace twelve, without alpha textures or new submissions. A geometry
regression checks bounds, opposite face normals, finite unit normals and the triangle budget.
Five focused render/rural/hay/urban/lakeside checks pass (0.92 seconds). The 52 km High capture
shows slender verge strands rather than blocks, but also confirms that the overall verge remains
too sparse; this silhouette correction is not a complete ground-cover solution. A subsequent
density pass uses nine blades per 6 m High group, five per 12 m Medium group and three per 50 m
alternating Low group. Deterministic lateral scatter, height and yaw variation break up each
2.1 m-wide clump. Each root is terrain-sampled and rejected near stations (240 m), water, tunnels,
bridges or urban sections, with chunk-boundary clipping. Matched nearly frozen 52 km captures
show a more continuous but still restrained verge. Six focused core/render/rural/urban/lakeside/
tunnel checks pass (4.16 seconds). High vegetation geometry increases, but submissions and loaded
chunk counts do not; dense botanical ground cover and motion-shimmer testing remain open.

Countryside silhouette audit: High window captures at 52 km exposed close conifers as perfectly
stacked straight-edged cones. Near-detail whorls now alternate recessed gaps and branch tips,
with variant/tier-dependent rotation, reach and vertical stagger. Crown radii stay within the
previous envelope, feature placement and simulation counts are unchanged, and distant trees keep
the cheaper cones. The recapture shows a broken branch outline; it is still visibly low-poly,
not realistic needle foliage. Five focused render/rural/hay/urban/lakeside checks pass (1.15 s).
Per-tier side count doubles only in the close-detail path, with no additional draw calls.
The 86 km approach capture also exposed an overly sparse horizon and broad empty settlement
plots; improving vegetation silhouettes alone does not address that larger composition gap.

Saloon luminaire correction: the original ceiling strip used the lit opaque interior material,
so its powered diffuser appeared dark brown depending on lighting. Recessed aluminium channels
now surround segmented warm-white diffuser faces in one shared unlit opaque batch for all six
cars (one added submission). Daytime and 123 km tunnel vestibule captures confirm stable emitting
surfaces. Five focused render/interior/tunnel/vestibule checks pass, followed by three Low-budget/
High-MSAA/material-integrity checks (7.94 seconds). Surrounding illumination still uses ambient
fill and existing gradients; this is not dynamic area lighting or bloom. A follow-up moves the
five gangway header diffusers and WC ceiling panel into the same emitting batch, retaining lit
metal frames and adding only twelve triangles, no further draw calls. The tunnel recapture shows
the header lamp lit alongside the saloon strip. Six focused render/WC/sink/interior/tunnel/vestibule
targets pass (3.16 seconds); the WC ceiling face itself is not framed by the current downward
sanitary inspection views.

Integrated validation after the avatar, platform and sanitary-geometry passes: the complete
configured CTest suite passes 53/53 in 95.88 seconds with four workers. New sink-preset coverage
checks occupancy, player placement and a closed door; another test rejects conflicting sink/WC
presets. Coverage also includes terminal reversal, station exchange, streamed scenery, audio,
avatar populations, material captures and install layout. CNA `next` at `97783d1e0` and
sharp-runtime `next` at `21c06910` had clean worktrees during this check. The configured renderer
selector remains `OPENGL33` with no multi-renderer selection, and compilation uses system ccache.
These automated/offscreen runs do not prove physical-mouse capture reliability, final visual
realism, physical-GPU performance or a long real-time manual journey.

Sanitary-fixture silhouette pass: the WC's straight cylinder and flat dark opening are replaced
by an oval tapered shell, actual recessed cavity, narrow pedestal and rounded rear housing.
The sink uses the same hollow profile instead of overlaid discs. These remain in the existing
interior batch with unchanged collision. The first capture exposed the pedestal protruding into
the cavity; lowering its top below the cavity floor removed the intersection in the recapture.
Core/render/WC/interaction checks passed, with render/WC/interaction repeated after that adjustment
(3/3, 2.96 seconds). The current WC camera shows the bowl clearly but faces away from the sink;
the initial sink inspection was therefore still open. The new `--sink-view` preset subsequently
exposed a bare vertical faucet intersecting the basin. A rear mounting ledge, relocated riser,
rounded spout, outlet, drain and wall support now make its construction legible. A closed-door
capture verifies the hollow basin and tap without door occlusion. Four focused core/render/WC/
interaction checks pass (3.90 seconds), and conflicting sink/WC presets are rejected. Smoother
porcelain shading was addressed next with analytic elliptical-cone normals on both shell faces:
the recapture removes the pronounced segment bands inside the WC bowl while preserving the
rim crease. A new MeshBuilder explicit-normal quad overload normalizes inputs and falls back
to the face normal for zero vectors; its regression checks direction, unit length and unchanged
clockwise indices. Render/WC/interaction targets pass (3/3, 2.15 seconds), without added geometry
or draws. A follow-up optional smooth ellipsoid mode removes both face-normal discontinuities
and baked longitude colour steps from the pedestal and platform suitcase wheels. Its regression
checks the analytic inverse-squared-radius surface gradient, unit normals, uniform vertex colour
and unchanged topology. The WC recapture confirms the pedestal improvement; render/WC/interaction
targets pass (3/3, 1.99 seconds). Default faceted vegetation remains unchanged. Porcelain specular
response, a less schematic mirror approximation
and sanitary-control detail remain open.

Uniform-colour state audit: `drawBox` now saves/restores the caller's BasicEffect diffuse colour.
Inspection found the same leakage pattern beyond platform luggage: the final exterior-door button
could tint the subsequently drawn vertex-coloured toilet door. Restoring at the shared helper
contains all its current callers (signal lenses, crossing lamps, clock hands, staff band and door
indicators), rather than relying solely on later white resets. Seven focused core/render/WC/door/
signal/crossing targets pass (3.53 seconds). A High vestibule capture was checked for regressions;
it does not frame the toilet door, so it is not a direct before/after proof of that surface colour.

Platform luggage polish: the shared case now has rounded shell corners, a narrow zipper gasket,
moulded ribs, metal handle tubes and four small wheel approximations, with its broad face oriented
toward the train. Inspection also identified a project-side BasicEffect state leak: the employee's
yellow band tinted following vertex-coloured luggage green. Explicit white diffuse state before
platform props restores their intended blue colour. A 180-frame High car-5 window capture shows
the new silhouette and corrected colour; five focused core/render/platform/Low/High targets pass
(26.78 seconds). This adds geometry to the existing shared mesh, not per-case draw calls.

Platform conversation audit: the old alternating fixed headings pointed the waiting pair away
from each other, while an unrelated phone user was also counted as a conversation partner.
Only the complete authored pair now receives social gestures; lower quality tiers cannot leave
a lone conversational actor. The pair's 6 m gap is reduced to 1.6 m and headings follow the actual
partner positions, including lateral stagger. Core/render/platform-view/Low-budget/High-population
checks pass (5/5, 22.37 seconds). A 180-frame High capture from car 5 shows the pair facing
each other at the new spacing; a canopy post partly obscured one person and motivated a follow-up
fixture-clearance pass. Waiting anchors now avoid canopy supports, lamps, benches and the ticket
machine while preserving conversation spacing across quality levels. Five focused targets pass
(25.18 seconds), and a new 180-frame High capture shows both bodies clear of the nearby support.
The subsequent exact segment/rectangle check also excludes fixtures between the partners. A new
180-frame High capture shows the pair together between supports, rather than talking through a
post. Low/High population and platform runtime checks passed; after moving an ambiguous boundary
point in the new regression 10 cm into free space, core/render checks passed again (2/2, 6.79 s).
Clearance for additional fixture families remains open. Platform figures remain lightweight waiting actors, not a
walking navigation system. Handset grip and stylized avatar proportions remain separate issues.

Seated-avatar orientation audit: a dedicated front-quarter aisle capture revealed that the
rendered torso and knees faced opposite directions. The project used the wrong forward-axis
assumption for CNA's public avatar API. Seat yaw and hip/knee signs are corrected together; the
recapture aligns face, torso, knees and front-rest hands. Three focused core/population/interaction
targets pass. A follow-up uses near-horizontal thighs (1.55 rad hip bend, opposite knee bend)
and a blended 10 cm forward pelvis offset: the inspected front-quarter capture now places the
shins beyond the cushion front rather than through it. Foot-floor fit still varies by avatar
proportions and remains open. Onboard walking, driver and attention-target yaw now share the
public -Z convention; an eight-direction regression protects that conversion. Core, rendering,
population and interaction targets pass (4/4, 18.82 seconds). This is not a manual walking or
camera-freeze validation. Rear-only screenshots and the previous numeric animation tests did
not establish pose correctness; platform avatars and individual activity poses still need audit.

Directional ground-shadow trial: Medium/High houses and townhouses now add bounded projected
silhouettes to the existing contact-shadow batch (six triangles each, no added draw). A production
window capture confirms restrained side/rear shadows; five focused rendering targets pass. These
are ground-only approximations with route-frame light direction and analytic terrain sampling,
not general cast shadows. Direction consistency on curves and terrain contact remain open; do
not treat RND-004 or realistic lighting as complete on the strength of this trial.

Residential density follow-up: the expanded route-wide industrial-compound test reproduced
townhouses occupying same-side yard reservations. Frontage now switches side, with a second
substation/compound check and omission if both sides are reserved; the focused five-target suite
passes. The km-2.6 capture also reveals trees intersecting residential facades. That vegetation
clearance defect was reproduced by a new crown/parcel regression and corrected for same-chunk
feature records using a deterministic post-generation filter. A matching km-2.6 capture no longer
shows the intersecting trees. A bounded eighteen-candidate relocation pass now recovers suitable
nearby planting sites while respecting parcels, pavement, terrain and other crowns; a recapture
shows trees between houses again. Adjacent raw parcel snapshots now also protect building
clearance across chunk seams independently of load order. The route-wide all-building crown test
and opposite-approach determinism test pass. Crown-to-crown overlaps across seams and separate
renderer-generated vegetation remain open; road-clearance checks alone do not cover them.

Urban grade follow-up: a km-2.3 passenger-window capture revealed that terrain relief could bury
the parallel road and parts of its pavement. The street surface existed and its draw statistics
passed, but neither established visibility. Urban terrain now reserves a level formation through
26 m on both sides, blending to natural relief by 40 m with explicit mesh bands. A same-camera
recapture visibly restores asphalt, road markings and pedestrian connections. Seven focused
urban/quality/material/rural/waterfront/crossing tests pass. Curved road subdivisions and chunk
boundary grades remain incompletely verified; this is not a blanket claim that all streets are
correct. Asphalt, kerbs, continuous edge markings and footways now use shared route endpoints
at at most 25 m spacing instead of one straight chunk-length box. Five focused urban, crossing,
High/Low and material checks pass, and a 90-frame window capture was inspected. The terrain's
coarser longitudinal tessellation and abrupt urban-envelope selection remain audit items.

### RND: rendering and material foundation

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| RND-001 | P0 / implemented | The audit baseline did not request MSAA, leaving window frames, catenary, seat edges, and UI-adjacent geometry visibly jagged. | Medium/High now request desktop MSAA through CNA, Low remains inexpensive, the applied sample count is reported, and both paths have OpenGL smoke coverage. Physical-GPU motion inspection remains part of final validation. |
| RND-002 | P0 / in progress | The baseline had no usable material or texture system despite carrying UVs. A deterministic procedural-image pipeline, mip generation, upload tests, explicit repeat UVs, quality-aware filtering, strict sampler restoration, and bounded terrain, ballast, concrete, carpet, seat-fabric, passenger-wall/ceiling-panel, resilient vestibule/WC-floor, lake/estuary-water, masonry/rendered-facade, polished-rail-head, and non-tiling cloud materials are implemented. | Extend the proven path to vegetation and more close fixtures, with physical-GPU motion validation. |
| RND-003 | P0 / in progress | The baseline shared effect treated every surface almost identically. High/Medium material passes now use per-pixel lighting and bounded material-specific specular power: textile remains nearly matte, concrete and resilient flooring are restrained, polymer panels have a tight soft response, and water/rail heads receive brighter controlled highlights. Low disables the extra response. | Add separate plastic, glass-adjacent trim, painted metal, and building families without multiplying draw calls; profile the High path on the physical target GPU. |
| RND-004 | P0 / in progress | Full-scene depth still relies heavily on face colour multipliers. Batched two-band contact darkening now grounds major procedural buildings, station canopies, every seat group, trees, and bushes; platform benches add their own bounded contact patches on Medium/High. The immutable interior shadow batch now also anchors all 33 table base plates and all six vestibule luggage/rack footprints with restrained local patches. The High smoke protects the resulting exact 264-triangle budget without adding a draw call. | Extend stable ambient-occlusion and directional cast-shadow cues to infrastructure and other close fixtures, then inspect overlap and exposure on the physical GPU. |
| RND-005 | P1 | The renderer is a 4,700-line translation unit that builds interior, track, world, stations, trains, avatars, and live displays. | Split by responsibility after rendering state and material ownership have stable interfaces; preserve batch efficiency. |
| RND-006 | P1 / in progress | The former opaque ten-point cloud polygons were visibly disk-like through passenger windows. The gradient sky now carries a deterministic non-tiling 256 px RGBA cloud material: eight overlapping analytic lobes, two scales of smooth value noise, vertically graded colour, and a feathered silhouette replace the hard edge. One transparent, renderer-independent, two-sided billboard batch supplies 4/6/8 Low/Medium/High banks; `sky_clouds` and a material-shape unit test guard the quality budgets. A High moving-window capture confirms soft contours and a clear horizon. The cards remain flat and lack true depth. | Add restrained depth-separated haze layers and validate their motion/parallax impression on the physical GPU without a costly volumetric pass. |
| RND-007 | P1 / in progress | Window tint remains intentionally clear. Medium/High separate 204 diagonal glints and upper-saloon ghost accents into two side-specific batches, with a fake Fresnel factor that is weakest face-on and stronger along the pane; Low omits them. Audit comparison found that the nearest old 17 cm ghost strip projected as a giant dark sky polygon from a window seat. Accent height/length, opacity, face-on multiplier, and vertical placement are now substantially reduced; an identical-camera High recapture removes the polygon while retaining barely visible glass cues. The approximation does not reflect actual scene content. | Validate reflection motion during a physical mouse ride and tune tunnel response; do not broaden the ghost until it remains below the landscape hierarchy from every seated camera. |
| RND-008 | P2 | There is no general authored glTF material path in the game. | Add a project-owned, optional glTF/CNJ asset lane only where authored geometry materially beats procedural construction. Keep gameplay independent of it. |
| RND-009 | P1 / corrected | The shared XZ-rounded box had reversed caps and wall winding relative to the ordinary box and other rounded variants. This exposed far-side vehicle wheels through the body and affected rounded furniture surfaces. The project-owned primitive is corrected without changing mesh counts. A new indexed-triangle orientation test reproduced the failure before the fix; all 25 render unit cases and seven focused runtime/integrity targets now pass. Urban and saloon OpenGL captures were inspected. | Keep the orientation regression for every box variant. Continue checking silhouette quality; this fixes a geometry defect, not the wider realism or physical-input validation gaps. |

### TRN: train interior and train body

Lighting follow-up (RND foundation): clockwise index ordering had been confused with outward
normal orientation. The shared normal helper lit inward-facing sides of boxes and terrain;
cylinders, cones, ellipsoids and horizontal discs also used inconsistent exterior winding.
Both are corrected inside the project, with unchanged geometry budgets. The expanded 27-case
render suite checks outward normals separately from indexed winding. Urban and saloon OpenGL
captures confirm the changed light response without increasing sunlight or ambient intensity.
This does not provide cast shadows or a physically based material model.

Follow-up geometry audit: all rounded-box planes omitted their final quarter-arc endpoint,
making nominally straight sides diagonal. The endpoints are restored and covered by a failing-
before/passing-after regression. Three-segment rounded solids now use 64 rather than 48 triangles,
with unchanged submissions and collision bounds. All six focused runtime/render/quality tests
pass, and a saloon capture confirms symmetric furniture profiles. This is a geometry correction;
the broader interior realism findings below remain open.

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| TRN-001 | P0 / in progress | Seat faces and cushion tops use woven detail, rear shells have recesses and pockets, contact shading grounds each group, and a faceted crown softens the headrest. Tested rounded primitives soften all 182 cushions, 182 upright back volumes, 93 aisle armrests, and 33 table tops. Each seat uses a slim central pedestal and under-cushion bracket instead of two broad plate legs; all 182 feet now expose two dark six-sided anchor bolts (364 total). Every table stem now terminates in a separate twelve-sided floor plate with two visible bolts (33 plates), complementing its wall bracket. Collision footprints and draw calls are unchanged, and exact telemetry is smoke-tested. Shell transitions remain procedural. | Continue improving shell transitions and seat-family variation while keeping the aisle collision envelope unchanged; physically inspect every seat bay during a complete six-car walk. |
| TRN-002 | P0 / in progress | Saloon wall/ceiling skins and upper WC walls now use subtle polymer grain; vestibules and the WC have a separate resilient aggregate floor with existing joints and anti-slip lanes retained above it. Thirty-six continuous ceiling-coving/skirting sections now terminate the saloon panels, 120 restrained transverse seams expose the modular ceiling construction, and 24 low-contrast wheel/scuff tracks break up the vestibule floor without raising its collision surface. All six safety-fixture end walls have shallow inset fields, perimeter rails, corner fasteners, and a dark three-sided portal seal. The shared automatic gangway leaf adds its own four-sided glazing gasket, brushed lower kick plate with ribs, and illuminated request sensor without changing the 1.24 m clear opening or adding a draw call; exact telemetry protects all three new detail populations and the six portals. Broad structural surfaces still lack localized wear variation. | Extend dirt-safe variation and material breakup through the remaining gangway/end-wall faces while keeping window exposure clean. |
| TRN-003 | P1 / in progress | All six cars carry distinct quiet, work/table, accessible, family, luggage, or standard signs and restrained upholstery/headrest/table accents. Close cues now include an open book, table laptops, an assistance-call panel, play blocks, a route-leaflet pocket, and a full-height luggage stack; the luggage car also carries the densest share of the 27 permitted overhead cases. A tested YZ-rounded primitive replaces the block silhouette of all 14 floor/rack cases; all six lower cases add aisle-facing zipper bands, U-shaped telescopic handles and grounded caster pairs while retaining the clear aisle. Exact telemetry protects both case and wheel-pair populations. The basic saloon shell and row pitch remain deliberately shared. | Develop stronger role-specific zoning and storage silhouettes while preserving aisle clearance, then validate recognition and traversal during the physical six-car walkthrough. |
| TRN-004 | P1 / in progress | Interior illumination now combines ambient light, a reduced sun contribution and two opposed diffuse ceiling-fill directions; opaque and textured batches share it. Medium/High also add 24 low-alpha ceiling/upper-wall gradients, while Low omits the gradient batch. Window-light projections remain depth masked. This is not spatially occluded illumination, and CNA avatars use separate lighting. | Improve fixture falloff and corner/vestibule response, reconcile avatar lighting, then inspect exposure and tunnel transitions on the physical GPU without obscuring scenery. |
| TRN-005 | P1 | The player train is held in one straight local frame. Curved world track is convincing through side windows, but the consist itself cannot articulate visibly. | Introduce per-car route poses for exterior-facing geometry and gangway deformation while retaining a stable passenger-local collision frame. |
| TRN-006 | P2 / in progress | Cab shells and the driver satisfy access restrictions. Both locked cabs now contain a crashworthy desk silhouette, rounded dual-screen bezel, six-button control row, combined power/brake handle, driver seat and paired interior windscreen wipers. Exactly one separately batched luminous dual-screen cluster follows the service direction; identical positive-cab captures before and after reversal visibly prove that it powers down when that end becomes trailing. `cab_control_groups=2` and `active_cab_displays=1` protect the invariant for one additional bounded draw. Side consoles and richer driver posture remain limited. | Add restrained side-console detail and active-end task posture while preserving the deliberately inaccessible passenger boundary. |

### STA: stations and railway infrastructure

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| STA-001 | P0 / in progress | Station families differ structurally. Their track-facing buildings combine a course-masonry plinth with a rendered-plaster upper facade, while passenger-facing openings use projecting frames, inset glazing, sills, and quality-scaled mullions. Six restrained terminal, suburban, rural, waterfront, urban, and market-town platform palettes now vary slab/base concrete, furniture, and bench finishes. | Broaden family-specific stone, glazing, roof, and edge-condition identities while holding the streamed facade path to its current two shared submissions. |
| STA-002 | P1 / in progress | Platforms include bounded bins, CCTV pods, ticket machines, drainage, lamps, clocks, markings, luggage, planters, poster cases, cabinets, and two minor/three major bench assemblies. A 6 m modular two-rail rear safety boundary follows every raised slab, leaves a building-aligned access opening, and is protected by `platform_boundaries`; rural/suburban families add restrained terrain-level planting beyond it. Every canopy now has two full-length gutters plus two rear downpipes terminating in floor shoes at the authored drainage channel; `canopy_drainage` protects the one assembly per loaded station in the existing streamed batch. Medium/High add five minor or seven major deterministic low-alpha wear patches plus bench contact shadows; Low omits the weathering batch. Three to six narrow palette-matched roof-runoff streaks age each track-facing station façade on Medium/High without transparency or another submission, while `station_facade_streaks` protects their bounded population. Furniture placement remains modular and sparse. | Strengthen station-family furniture compositions without losing visual hierarchy or exceeding the bounded static submission. |
| STA-003 | P1 / in progress | The quality-scaled station pool now forms uneven deterministic travel pairs with lateral spacing, pair-facing orientations, a track-facing employee, and paired luggage ownership. Major-station pairs add subdued alternating arm/head gestures, while one stable train-centre waiting position carries a handset attached to the CNA wrist hierarchy instead of a guessed body-space point. The live board was reduced from a 3.70 x 1.32 m billboard-like slab to 3.05 x 1.08 m, moved behind the principal waiting line, and offset enough to preserve a window sightline while retaining all route-aware text. It remains a small fixed waiting pool rather than a complete platform flow model. | Add door-target choice, arrival/departure walking, and density profiles by station importance. |
| STA-004 | P1 / in progress | Mipmapped ballast now sits below three-part rails with an oxidized foot/web and a separately lit wheel-polished head; concrete sleeper tones vary deterministically. Every near-field rail/sleeper junction has a batched dark baseplate and paired rust-toned elastic clips, with a quality-scaled 360/460/560 m build radius; catenary poles sit on concrete foundations. The messenger follows a 50 m parabolic sag approximation with matching variable droppers, while each mast adds a diagonal brace and staggered registration assembly. Deterministic 420 m signalling/telecom groups alternate side away from stations, tunnels, bridges, and water: four restrained cabinet finishes share a plinth, inset door, three louvres, warning plate, cable marker, and Medium/High contact patch. A representative rural window reported five `lineside_cabinets` in the existing static submission. | Add route-authored equipment concentrations and further restrained condition variation; profile the denser foreground on the physical target GPU. |
| STA-005 | P2 / in progress | Major-station sidings diverge smoothly from the main alignment and can hold a parked three-car service, but remain non-dispatched scenery. Each in-range approach adds a deterministic concrete-mounted point motor, drive rod, post and yellow route indicator beside the geometric merge. Paired 22 m polished switch blades now follow the real easing function, while each dark-based frog is solved at the first 1.52 m siding separation and receives crossing main/branch noses. `turnout_motors` and `turnout_blades` expose matching recentered populations without another submission. Yard termination remains absent. | Add selected buffer stops and route-consistent yard compositions without implying a dispatch simulation; obtain a close physical-GPU inspection angle for the low rail detail. |

### WLD: route environment and streaming

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| WLD-001 | P0 / in progress | Countryside foregrounds now use a distinct muted railway-verge domain and actual 34 cm terrain depressions outside both fences, sampled by three cross-corridor vertices every 50 m. Quality-scaled deterministic grass tufts soften the outer banks. Rural tint cells span coherent 500 m parcels, whose boundaries receive low terrain-following segmented hedges. Two established random crop parcels remain unchanged, while a separately hashed third field alternates route side per eligible chunk; this prevents multi-kilometre passenger-side gaps without perturbing later landmark seeds. A focused invariant requires at least five visible fields per side in a seven-chunk rural window. Urban, station, tunnel, bridge, and lakeside sections reject the relevant rural systems. | Add authored embankment cuts and extend parcel variation beyond the current bounded palette. |
| WLD-002 | P0 / in progress | Near broadleaf crowns remain lobed and visibly faceted. Close conifers now use seven whorls of branch fans around a narrow core; distant trees retain three simplified tiers. Stable route-position identities vary crown breadth, branch phase and whorl height. Medium/High close conifers have shared mipmapped needle detail; broadleaf foliage has its own shared material. Quality-bounded terrain-following understorey and contact patches ground nearby trees, while the maintained railway corridor stays clear. | Improve natural crown silhouettes and ground-cover transitions. Measure rebuild cost and moving GPU views before increasing density: recent rural geometry is substantially heavier than the old 431,708-triangle audit sample. |
| WLD-003 | P1 / measured, still open | Seven chunks are bounded, but the complete static-world mesh is rebuilt and uploaded on every 250 m transition. Opaque, broadleaf and needle CPU staging now retain capacity. The latest 1,800-frame private GPU sample still measured a 45.0 ms maximum rebuild and 54.7 ms maximum steady frame; typical frame time did not improve. | Cache/reuse chunk geometry or stage rebuild work, then repeat transition and input measurements. Capacity reuse alone does not eliminate rhythmic travel stutter. |
| WLD-004 | P1 / in progress | Houses, apartments, and station upper walls now expose rendered plaster, while townhouses and station plinths retain masonry. Passenger-facing windows have inset glazing, projecting frames/sills, and near-field mullions. Houses, townhouses, apartments, warehouses, and the lower/stepped volumes of distant towers repeat distance-scaled framed window detail across both route-facing end walls; dedicated tower telemetry prevents a return to blank high-rise slabs. Dense-area houses/townhouses now sit on bounded planted plots, apartments/towers on paved forecourts, and all four families expose a threshold and short entrance path; the latter two also gained their missing framed doors. A representative High window reports 123 plot/path groups in the existing batch. Warehouses use four restrained industrial palettes plus plinths, loading aprons, framed slatted shutters, docks, clerestories, cladding ribs, service bands, and rooflights. Random standalone frequency is reduced; deterministic two/three-building compounds share curved hardstanding, a gated front fence, marked parking, pallet stacks, mildly varied building scale, and one detailed delivery truck. Every warehouse carries rear-wall louvred plant, a downpipe, and paired roof extractors; three-building premises add a glazed wheeled service van plus one detached box trailer with rear-door ribs, tandem wheels and landing legs. Two raised fence-edge beds per compound add irregular evergreen shrubs, quality-scaled secondary foliage and contact shading while remaining clear of the gate and service paths. A representative High urban window now reports two compounds, four yard vehicles, one explicit trailer and four planted buffers. Mirrored urban roads now serve both sides with distributed traffic, continuous edge lines, outer footways, lighting and sparse slotted kerb inlets; a seven-chunk High window reports 28 edge sections and 48 grates. Building footprints stay beyond 25 m and urban vegetation beyond 26.2 m, preventing street overlap. Yards still lack workers. | Add bounded loading-worker cues and broader transitions between industrial, residential, and rural land uses. |
| WLD-005 | P1 / in progress | Distant terrain bands use a lighter desaturated aerial-perspective palette. Rural chunks add a quality-scaled woodland line at 310-422 m plus broad faceted ridge masses beyond it: Low alternates one side, Medium fills both sides, and High adds a farther second layer. Urban/suburban chunks instead add muted settlement silhouettes. A representative High rural view produced 28 ridge masses and no empty horizon, although their silhouette remains visibly procedural. | Improve ridge scale/shape variation and blending, then validate horizon motion, fog transitions, and parallax on the physical GPU. |
| WLD-006 | P1 / in progress | Lake, estuary, and the Lakehurst/Redwater station waterfronts now share a deterministic mipmapped directional-wave material and controlled sun response. The lake adds shore-to-depth tint, reeds, glints, a carved basin, and a wooded far bank; the estuary adds five restrained depth/shore colour bands, while each waterfront uses three promenade-to-depth bands. | Add exposed shore and wet-bank transitions, bridge shadowing, and less periodic secondary wave variation. |
| WLD-007 | P2 / in progress | Other trains and road traffic remain primarily decorative. Urban traffic is now split deterministically across mirrored streets while preserving two opposing lane directions and the same bounded population. Each shared car mesh gained rounded body/cabin corners, sloped two-sided windscreens, side mirrors and contrasting hub cues in addition to its existing glazing, lamps and bumpers; a production capture removes the former pure-cuboid silhouette without adding submissions. | Add route-consistent spawn windows, station parking logic, road grouping and more body archetype variation while keeping CPU cost bounded. |

### NPC: passengers, conductor, and driver

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| NPC-001 | P0 / in progress | The normal service reports 38 logical actors including staff and the offboard exchange reserve. Persistent `(id, appearance)` visual slots use four-metre selection hysteresis. High prepares 18 shared avatar visuals, not one model per logical actor. Civilian selection now considers at most 128 untouched descriptions, prioritizing restrained tops/trousers and ordinary headwear. Fresh poses use a 0.74 head-bone scale. Random identities and stylized facial geometry remain; clothing/proportion adjustments do not produce realistic human assets. | Prioritize facial/garment asset quality and stable visible identity. Inspect continuity, accessories, seat contact and animation during a physical six-car walk before closing this finding. |
| NPC-002 | P0 / in progress | Locomotion uses procedural bone layers rather than dedicated clips. Gait phase now advances from measured train-local actor displacement rather than global time: blocked actors and frozen render-only frames settle the 0.18/0.24 s blend instead of walking on the spot, discontinuous teleports are rejected, and identity-derived initial phase preserves crowd variety. Hip/knee/shoulder motion now includes restrained ankle counter-rotation and toe roll. During the 0.55 s exit blend, the rendered pelvis remains weighted toward the reserved source seat, turns toward the aisle, adds a bounded lower-back weight transfer, and converges on authoritative navigation before standing completes; the inverse sit-down blend receives the same restrained mid-transition weight shift. AI, collision, and reservation state remain unchanged. `npc_seat_transitions` exposes live activation. A moving-car capture reported one visible distance-driven gait layer, while deterministic unit tests cover motion, blocking, initialization, teleport rejection, phase wrap, seat-exit placement, and bidirectional transfer weighting. Final posture alignment remains approximate. | Add turn anticipation, final seat-approach alignment, hand support where geometry permits, and visual foot-contact validation on the physical GPU. |
| NPC-003 | P1 | Navigation is a longitudinal lane model with targeted avoidance, not a full interior navigation graph. | Add doorway, vestibule, gangway, toilet, seat-row, and waiting nodes with reservations and local passing rules. |
| NPC-004 | P1 / in progress | Social behavior is stateful. Persistent visuals blend bounded head yaw/pitch, seated conversation partners turn toward each other, window-watchers scan the view, and a deterministic subset uses a two-handed reading pose. Another bounded subset of neutral seated/standing riders raises its arms around a dedicated screened handset and looks down at it; `npc_phone` telemetry protects the behavior. Initial activity residuals are now identity-phased instead of imposing a synchronized twenty-second quiet period. A small bounded activity share leaves a passenger standing safely in the aisle beside the source row; the renderer pairs that settled actor with the nearest unclaimed seated rider in the same car, supplies reciprocal eye contact, turns only the standing body, and adds a restrained single-arm gesture. Pairing excludes seat-exit transfers and actually moving vestibule actors. `npc_social` counts visible seated and standing conversations. Nearby stationary passengers and the conductor acknowledge the player with bounded gaze and restrained torso follow. A proposed variable collision radius was rejected after long integration tests exposed degraded station exchange, so navigation safety remains unchanged. | Add context-aware acknowledgement cooldowns and visual personal-space variation without changing the proven narrow-aisle clearance invariant; physically inspect a naturally occurring standing conversation. |
| NPC-005 | P1 | Station exchange demonstrates boarding/alighting but not strong destination intent. | Assign journey legs, luggage probability, preferred doors/cars, and preparation timing from route state. |
| NPC-006 | P2 / in progress | The conductor performs rounds, inspections, station-door duty, player-aware acknowledgement, and non-blocking service-information interaction. A dedicated rounded handheld terminal with screen, status bar, keys, and wrist loop now drives a two-arm inspection pose or clips at the side while walking. Spoken greeting and richer duty gestures remain absent. | Add restrained greeting feedback, platform-facing dispatch poses, and more legible but non-intrusive inspection timing. |

### PLY: first-person interaction and comfort

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| PLY-001 | P0 / in progress | Relative samples are decoded as deltas. Capture reconciles project state, CNA native relative mode and actual window activity; focus loss or stale-state mismatch tears down native mode, while overlays use an intentional release path. Unit tests cover policy transitions. A four-minute private-X11 avatar-enabled probe verified mouse motion through twelve pause/map/focus cycles, thirteen acquisitions and four deliberate focus losses. The intermittent freeze was not reproduced; physical hardware and the user's desktop compositor remain unverified. | Complete and record a real desktop mouse session including capture, focus loss, reacquisition, long continuous turns, and every car. |
| PLY-002 | P1 | Collision is intentionally simple and mostly horizontal. | Add swept movement/substeps for low frame rates and more explicit dynamic passenger clearance without trapping the player. |
| PLY-003 | P1 | Seat interaction is robust but the sit/stand transition has no body presence. | Improve easing, view target, standing clearance feedback, and nearby passenger response. |
| PLY-004 | P2 | Interaction prompts are functional but visually utilitarian. | Add unobtrusive iconography, consistent target labels, accessibility scaling, and clearer unavailable reasons. |

### AUD: railway and passenger audio

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| AUD-001 | P0 | Procedural loops react correctly but their short synthetic character can become recognizable during a long journey. | Expand non-periodic texture layers, crossfade variants, randomize phase/pitch conservatively, and validate loop seams. |
| AUD-002 | P1 | Train audio is route-state aware but not sufficiently car-position aware. | Vary bogie, traction, HVAC, gangway, vestibule, and toilet balance by player car and local position. |
| AUD-003 | P1 | Passenger murmur is spatial and irregular but has few source variants. | Generate a larger CC0/project-owned nonsense corpus with age/timbre/distance variation and stronger overlap control. |
| AUD-004 | P1 | Announcements are complete but synthetic prosody and acoustic treatment need a human listening pass. | Improve phrasing, station-name stress, PA filtering, cabin reverb approximation, levels, and caption timing. |
| AUD-005 | P2 | Track texture is broadly speed-reactive rather than route-surface authored. | Add deterministic welded rail, bridge, turnout, tunnel, curve, and rough-section events tied to route metadata. |

### UIA: passenger information, route map, settings, and accessibility

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| UIA-001 | P1 | Information is correct and readable, but the pixel-font presentation dominates the visual identity. | Keep clarity while adding hierarchy, alignment, restrained icons, better scaling, and hardware-display imperfections only where subtle. |
| UIA-002 | P1 | The map is useful but linear and visually detached from geography. | Add restrained route geography/landmark context while keeping the complete service leg readable. |
| UIA-003 | P1 | Comfort options cover FOV, sensitivity, and vibration; graphics control is coarse. | Add antialiasing, material detail, shadow approximation, population, and render-distance controls with safe presets. |
| UIA-004 | P2 | Keyboard help exists, but there is no remapping or broader accessibility layer. | Add remappable actions, prompt scale, colour-safe state cues, and optional hold/toggle interaction behavior. |

### ENG: architecture, performance, validation, and release readiness

| ID | Priority | Finding | Required direction |
|---|---|---|---|
| ENG-001 | P0 | Visual quality changes lack quantitative image/performance budgets beyond integrity checks and object counts. | Record applied MSAA, frame-time percentiles, chunk rebuild time, texture memory, and representative screenshot comparisons. |
| ENG-002 | P1 | `SceneRenderer` and `TrainSimulatorGame` concentrate too many responsibilities. | Extract materials, interior, route-world, station, avatar, and overlay components incrementally with no rewrite. |
| ENG-003 | P1 / in progress | Renderer tests prove presence and state, but visual-composition coverage remains limited. A High-quality material capture now rejects a predominantly black or tonally collapsed passenger-eye scene without exact-image matching. | Extend deterministic checks to silhouettes, material activation, foreground coverage, and representative station/outdoor scenes. |
| ENG-004 | P1 | The project has no committed history yet, so every file appears untracked. | Create a coherent initial commit only when the repository owner requests it; never include build products. |
| ENG-005 | P2 | Linux desktop is the only tested target, as intended, but renderer-independent boundaries are mostly conventional. | Keep new simulation/material descriptions free of OpenGL calls and confine CNA graphics ownership to render modules. |

## Ordered production campaigns

The ordering below is intentional. A campaign is not complete merely because its code compiles.
Every campaign requires a running visual inspection and regression coverage.

1. **Image foundation:** desktop MSAA, applied-sample diagnostics, procedural material texture
   infrastructure, mip/filter policy, and screenshot/performance baselines.
2. **Passenger eye-level interior:** seats, carpet, resilient flooring, wall/ceiling panels,
   vestibules, gangways, toilet materials, local depth cues, and window treatment.
3. **Trackside foreground:** ballast, rail/sleeper breakup, verges, embankments, vegetation
   silhouettes, field boundaries, and water edges.
4. **Station identity:** material families, platform furniture/equipment, grounded canopy/building
   shadows, signs, crowds, and boarding composition.
5. **Human motion and intent:** stable avatar assignment, navigation graph, locomotion/seat/turn
   transitions, gaze, social idles, journey legs, conductor presence, and station flows.
6. **Large-world composition:** settlement grammar, authored landmark scenes, distant layers,
   other-train scheduling, traffic grouping, and chunk-build stutter removal.
7. **Soundscape depth:** longer and less repetitive train layers, car-local acoustics, richer
   murmur, PA treatment, and route-authored rail events.
8. **Polish and release discipline:** settings, accessibility, UI art direction, profiling,
   physical mouse validation, long real-time journeys, memory checks, packaging, and updated
   showcase captures.

## Acceptance criteria for the realism campaign

- First-person screenshots must no longer be dominated by large perfectly flat single-colour
  surfaces at the seat, wall, floor, platform, terrain, or building scales.
- Thin geometry must remain stable in motion, with verified desktop antialiasing and mipmapped
  material detail rather than distant shimmer.
- Every station family must be identifiable from an unlabelled window view.
- A complete walk through all six cars must show meaningful functional and material variation,
  with no blocked gangway, camera jitter, collision leak, or NPC deadlock.
- Nearby passengers must retain identity, avoid the player, enter/leave seats and doors cleanly,
  and exhibit varied believable activity during a normal station interval.
- Chunk transitions must not create a periodic visible hitch on the target desktop profile.
- A normal endpoint-to-endpoint journey and reversal must retain bounded chunks, passengers,
  audio instances, GPU resources, and correct service state.
- The full automated suite, visible OpenGL 3.3 runs, focused static analysis, screenshots, and
  dependency cleanliness checks remain mandatory after each substantial campaign.
