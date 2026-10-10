# Screenshot-based environment audit

Baseline: 2026-10-09, CNA `next` / OpenGL 3.3, High quality, 1280 × 720.
This is a new environment audit, not a claim that the earlier development audit's
items are resolved. **120 visible findings** are ordered by passenger exposure
and visual prominence, with close inspection and exterior-only findings last.
All start OPEN; a fix must have a separate evidence entry before closing it.
Related construction details are separate only when they need different work.

The baseline has **three additional findings**, ENV-121 from ceiling close views,
ENV-122 from canopy inspection and ENV-123 from shoreline refinement.
Total scope is **123 findings**; original IDs are stable.

Current disposition: **107 addressed, 16 open**. ENV-001 through ENV-104 and ENV-121 through ENV-123 have
repair evidence below; this does not close broader glazing or fabric realism.

## Evidence and limits

The 34 actual engine screenshots and corresponding logs are in
`build/visual-audit/`. [Capture script](../tools/capture_visual_audit.sh) records
the exact poses and a binary SHA-256 receipt. No images were painted or generated
to stand in for engine output. Images are local ignored QA artifacts, not source
assets. Preserve this baseline when capturing fixes: use different filenames.

Run the script on an isolated X11 display or SDL offscreen. The finite-run
`--audit-camera X Y Z YAW PITCH` override uses train-local metres, render-space
height (including the train's floor-above-rail offset), and angles in degrees.
It requires both `--frames` and `--screenshot`; it does not unlock gameplay exit,
change player collision, or add an exterior player camera.

Captures use three frames, effectively frozen route time, no audio, no avatars,
and the existing vibration-free comparison configuration. Absence of people is
therefore **not** a finding. These images cannot establish animation quality,
flicker, long-run performance, audio, interaction correctness, or camera-freeze
resolution. Three-frame startup timings are not an FPS benchmark. The
`passing-train` frame did not capture a passing train; that subject remains
unassessed in this set. The tunnel frame shows the interior wall, not its portal.

Evidence keys below are filenames without `.png`, relative to
[the capture directory](../build/visual-audit/):

| Group | Inspected frames |
| --- | --- |
| Every passenger car | `car-1-aisle`, `car-2-aisle`, `car-3-aisle`, `car-4-aisle`, `car-5-aisle`, `car-6-aisle` |
| Seating and windows | `seat-front-close`, `seat-rear-close`, `window-oblique`, `window-corner`, `window-outside` |
| Compartments | `toilet`, `sink`, `gangway`, `vestibule`, `doors-platform` |
| Train exterior | `train-side`, `train-front`, `train-wide` |
| Stations | `terminal-platform`, `city-platform`, `rural-platform`, `waterfront-platform`, `terminal-wide` |
| Landscape and infrastructure | `urban-right`, `urban-left`, `country-near`, `lake`, `tunnel`, `river-bridge`, `viaduct`, `signal`, `track-close`, `passing-train` |

P1 = persistent/near passenger view; P2 = prominent station or route view;
P3 = close inspection; P4 = primarily exterior audit view. Priority is visibility,
not a claim that every item is a correctness defect. Material and design judgments
are explicitly visual observations, not assertions that underlying systems do
not exist. "Not visible" is limited to the cited view.

## Ranked findings

| Rank / ID | Priority | Visible problem and intended acceptance direction | Evidence |
| --- | --- | --- | --- |
| 001 ENV-001 | P1 | Window bays have no continuous dark perimeter gasket; sky against bare posts reads as an unsealed opening. Add a closed seal around each pane. | window-corner, window-oblique |
| 002 ENV-002 | P1 | Window-to-header and sill junctions lack a recessed reveal; square butt joints look unfinished. Give the aperture a continuous interior liner. | window-corner, window-outside |
| 003 ENV-003 | P1 | Window corners are hard square cutouts without shaped infill; make rounded, closed aperture corners without obscuring scenery. | window-corner, car-2-aisle |
| 004 ENV-004 | P1 | Large cushion front and side surfaces are uniform blue despite the textured top. Carry correctly scaled upholstery around the cushion. | seat-front-close |
| 005 ENV-005 | P1 | Backrest shoulder/crown upholstery is flat coloured around the inset fabric. Continue fabric across upholstered surfaces, not the plastic shell. | window-oblique, car-2-aisle |
| 006 ENV-006 | P1 | Existing seat fabric reads as a regular checker/grid rather than woven moquette. Separate small weave from larger restrained yarn pattern. | car-2-aisle, seat-front-close |
| 007 ENV-007 | P1 | Cushion tops look like thick flat blocks without a sitting depression or soft front roll. Improve the upholstered profile within collision bounds. | seat-front-close |
| 008 ENV-008 | P1 | Headrest crowns have conspicuous polygonal shoulders. Improve their silhouette at passenger distance. | car-6-aisle, window-oblique |
| 009 ENV-009 | P1 | Headrest covers read as flat pale stickers. Add cloth edge, modest drape and fold shading. | car-2-aisle, car-4-aisle |
| 010 ENV-010 | P1 | Rear seat shells are large featureless dark slabs. Add restrained moulded-plastic grain and panel shaping. | seat-rear-close |
| 011 ENV-011 | P1 | Glass reflection includes obvious rectangular/diagonal overlay boundaries. Remove geometric patch edges while keeping the view clear. | window-oblique, signal |
| 012 ENV-012 | P1 | Teal window mullions look like exposed square structural bars instead of finished passenger trim. Add differentiated cap/reveal surfaces. | window-corner, car-1-aisle |
| 013 ENV-013 | P1 | Armrests read as flat rigid rectangular bars, not comfortable padded supports. Refine profile and material separation. | seat-front-close, car-2-aisle |
| 014 ENV-014 | P1 | Tables are uniformly pale slabs with no convincing laminate surface response. Add subtle material variation at sensible scale. | car-2-aisle |
| 015 ENV-015 | P1 | Table edges are thick square bands. Add rounded edge/bevel and credible laminate edge treatment. | seat-front-close |
| 016 ENV-016 | P1 | Cup recesses read as grey polygon patches rather than recessed holders. Give them a readable lip and concavity. | car-2-aisle |
| 017 ENV-017 | P1 | Central ceiling lighting is an uninterrupted white rectangle. Add believable diffuser and fixture segmentation. | car-1-aisle, car-2-aisle |
| 018 ENV-018 | P1 | Interior lighting makes furniture broadly uniform in brightness; seat/fixture contact is weak. Improve local grounding without expensive per-seat lights. | seat-front-close, car-2-aisle |
| 019 ENV-019 | P1 | Carpet is visually homogeneous across heavy-use and protected areas. Add restrained spatial wear variation, not random dirt everywhere. | seat-front-close, vestibule |
| 020 ENV-020 | P1 | Aisle inlays have long hard graphic borders with little seam/termination detail. Integrate them with the floor construction. | car-2-aisle, seat-front-close |
| 021 ENV-021 | P1 | Grab handles are rigid square yellow U shapes. Round the grip and distinguish hanger from hand-contact material. | car-1-aisle |
| 022 ENV-022 | P1 | Luggage racks use chunky parallel rectangular bars, visually dominating the window top. Refine shelf/lip proportions. | window-oblique, car-2-aisle |
| 023 ENV-023 | P1 | Rack brackets are large blank L plates without convincing joints. Add credible attachment details without separate draw calls. | window-corner |
| 024 ENV-024 | P1 | Passenger information screens are very wide flat rectangles with little physical bezel depth. Model a coherent housing. | car-2-aisle |
| 025 ENV-025 | P1 | Information typography is coarse all-capitals pixel lettering even at close distance. Improve legibility and hierarchy while retaining dynamic content. | car-2-aisle, doors-platform |
| 026 ENV-026 | P1 | Passenger doors have large flat teal panels and pale rectangular glazing without clear glass/reveal separation. Give door windows their own sealed construction. | vestibule, track-close |
| 027 ENV-027 | P1 | Door controls read as tiny coloured blocks without recognizable button faces or legends. Add readable tactile/control cues. | vestibule, gangway |
| 028 ENV-028 | P1 | Gangway bellows read as nested rigid rectangular frames. Add a convincing flexible folded surface and depth. | gangway |
| 029 ENV-029 | P1 | Large vestibule end-wall panels are flat black fields. Improve panel material and edge articulation. | vestibule, car-6-aisle |
| 030 ENV-030 | P1 | Car roles have weak visual differentiation: family, accessible and luggage cars repeat the same foreground furniture language. Strengthen role cues without clutter. | car-3-aisle, car-4-aisle, car-5-aisle |
| 031 ENV-031 | P2 | Some broadleaf upper trunks/branches visibly float above the lower trunk. Close the discontinuity for every seeded tree scale. | urban-left, window-oblique |
| 032 ENV-032 | P2 | Broadleaf crowns remain solid clustered lumps despite leaf detail. Improve silhouette porosity before adding more noise. | country-near, passing-train |
| 033 ENV-033 | P2 | Conifer branch layers read as rigid angular skirts. Improve branch silhouette and needle coverage economically. | country-near, urban-right |
| 034 ENV-034 | P2 | Small shrubs are green polygon blocks with sharp flat faces. Replace the block silhouette with low-cost foliage. | country-near, passing-train |
| 035 ENV-035 | P2 | Hedges look like extruded angular walls, especially their exposed ends. Add leafy end caps and less mechanical profiles. | country-near, passing-train |
| 036 ENV-036 | P2 | Grass tufts are sparse isolated dark spikes, not convincing verge cover. Improve blade clusters and density distribution. | country-near |
| 037 ENV-037 | ADDRESSED | Rural parcel edges now blend through narrow grass margins and restrained soil variation; see repair evidence below. | viaduct, river-bridge |
| 038 ENV-038 | ADDRESSED | Rural trees now form irregular copses and clearings with occasional edge solitaires; see evidence below. | river-bridge, viaduct |
| 039 ENV-039 | ADDRESSED | Irregular height-field hills replace ellipsoids, with terrain-sampled buried skirts and no underside. | river-bridge, country-near |
| 040 ENV-040 | ADDRESSED | Four distinct cloud silhouettes share one atlas with varied elevation and aspect ratio. | lake, river-bridge |
| 041 ENV-041 | ADDRESSED | Budget-neutral paired street blocks, shared parcel passages and garden boundaries replace isolated frontage spacing. | urban-left, urban-right |
| 042 ENV-042 | ADDRESSED | Inset graded panes and deterministic room/blind/curtain variation replace identical residential blue panels. | urban-right |
| 043 ENV-043 | ADDRESSED | Near flat roofs have textured membrane, coping, seams, drains and vents; pitched tile variation is restrained and less regular. | urban-right, lake |
| 044 ENV-044 | ADDRESSED | Closed roof soffits remove the sky gap; near houses have open trough gutters, supports, connected downpipes and outlets. | lake, urban-right, eave-drainage-eave-close-before |
| 045 ENV-045 | ADDRESSED | Near residential entrances have frames, panels, thresholds and hardware; overlapping ground-floor windows are removed. | urban-left, lake |
| 046 ENV-046 | ADDRESSED | Grain, near slab joints and chamfered kerbs distinguish road and pavement; the outer footway now meets its kerb. | urban-right, lake |
| 047 ENV-047 | ADDRESSED | Existing street/parcel joins now have textured approaches, low edging and bounded planting; access corridors are reserved against tree crowns. | urban-right |
| 048 ENV-048 | ADDRESSED | Tapered body, lined wheel arches, graded glazing, door hardware and five-spoke hubs improve the shared traffic prop. | urban-left |
| 049 ENV-049 | ADDRESSED | Station names, platform identifiers and live boards share flat antialiased proportional lettering instead of extruded pixel blocks. | terminal-platform, city-platform |
| 050 ENV-050 | ADDRESSED | Compact supported boards repeat along the platform, with centre placement checked for seated readability and entrance clearance; platform numbers no longer mask live boards. | city-platform, rural-platform |
| 051 ENV-051 | ADDRESSED | Boarded rural/waterfront waiting halls, entrance hoods, urban piers and different window proportions distinguish station facade families. | city-platform, rural-platform |
| 052 ENV-052 | ADDRESSED | Smaller textured soffit panels, steel cross-beams, head plates and grounded bolted columns give the canopy supporting construction. | terminal-platform, rural-platform |
| 053 ENV-053 | ADDRESSED | Mounted chamfered housings, dark seals, fasteners and separate warm-white diffusers replace flat strips. | rural-platform, waterfront-platform |
| 054 ENV-054 | ADDRESSED | Staggered 600 mm paving, narrow joints and shallow bevels replace the oversized platform grid on Medium/High. | city-platform, doors-platform |
| 055 ENV-055 | ADDRESSED | 400 mm tactile tiles carry small raised blisters; wider pale coping distinguishes the platform edge from the warning line. | doors-platform |
| 056 ENV-056 | ADDRESSED | Bins have genuinely recessed mouths, sloped lids, folded cheeks, service doors, locks and readable labels. | city-platform, terminal-platform |
| 057 ENV-057 | ADDRESSED | Rails connect between capped, grounded posts; both fence-run ends and access-gap edges receive explicit supports. | terminal-platform, waterfront-platform |
| 058 ENV-058 | ADDRESSED | Framed station windows use inset waiting-room/office cues and muted reflection gradients; entrance-door glazing follows the same palette. | city-platform, rural-platform |
| 059 ENV-059 | ADDRESSED | Station-family palettes separate brick or render, darker plinths, coping, cornices and corner masonry while preserving entrance clearance. | city-platform, rural-platform |
| 060 ENV-060 | ADDRESSED | The blank mass is now a brick clock turret with a visible functioning face, louvres, corner masonry and a finished service door. | doors-platform |
| 061 ENV-061 | ADDRESSED | Live boards have closed chamfered housings, inset screens, frame fasteners, rear brackets and bolted foot plates. | waterfront-platform |
| 061b ENV-122 | ADDRESSED | The complete live board fits below the beams and between columns; its text is no longer clipped by the canopy. | canopy-board-clearance-before |
| 062 ENV-062 | ADDRESSED | Station-to-city background is a broad empty strip rather than a credible forecourt. Add a coherent pedestrian/service transition. | terminal-platform, terminal-wide |
| 063 ENV-063 | ADDRESSED | Water reads as a uniform blue sheet from the lake window. Improve view-dependent surface variation without sacrificing clarity/performance. | lake |
| 064 ENV-064 | ADDRESSED | Lake shoreline is a hard horizontal meeting of water and ground. Add a grounded bank profile/material transition. | lake |
| 064b ENV-123 | ADDRESSED | The far lake bank ends at the streamed terrain edge before fog fully hides it in oblique views. Keep a bounded background surface beyond the fog distance. | lake-bank-stream-edge-discovery |
| 065 ENV-065 | ADDRESSED | River far bank is a straight cut with fields immediately reaching it. Add bank vegetation and irregularity. | river-bridge |
| 066 ENV-066 | ADDRESSED | Bright water streaks are long isolated straight lines, conspicuous as graphics. Refine highlights into coherent surface response. | river-bridge |
| 067 ENV-067 | ADDRESSED | Both crossings now have a concrete upstand with coping and drip line, a cable trough and a slender tubular two-rail handrail on posts instead of stacked flat beams. | river-bridge, viaduct |
| 068 ENV-068 | ADDRESSED | The viaduct crosses a bowl-shaped valley with rising spurs and tall footed, tied piers rather than a uniformly sunken plain. | viaduct |
| 069 ENV-069 | ADDRESSED | The lining is a textured precast face with 2 m ring joints, bolt pockets, longitudinal joints and a roof haunch instead of a flat slab. | tunnel |
| 070 ENV-070 | ADDRESSED | Wall fixtures are bracketed housings with end caps and a thin lens; a seamless gradient wash on the lining gives the falloff. | tunnel |
| 071 ENV-071 | ADDRESSED | Handrail and cable tray run on brackets with cables; every bay carries an escape-route sign with portal distance and a hectometre plate. | tunnel |
| 072 ENV-072 | ADDRESSED | Ballast is now size-varied angular aggregate with per-stone facet lighting and thin crevices at a 0.88 m tile, no longer a cobbled grid. | signal, track-close |
| 073 ENV-073 | ADDRESSED | Both shoulders slope from the crown to a deterministically wobbling toe in the ballast material; the formation box is hidden beneath. | track-close |
| 074 ENV-074 | ADDRESSED | Near sleepers are chamfered monoblocks with two rail-seat blocks and a lower waist, keeping the three-tone variation; far ones stay boxes. | urban-left, track-close |
| 075 ENV-075 | ADDRESSED | Rails have an oxidised foot, a slender web and a broader steel head with the polished running surface on top. | track-close, signal |
| 076 ENV-076 | ADDRESSED | Fences are slim 3 m posts with four wire strands following the route chord, closed by strained end posts with struts where the fence stops. | passing-train, country-near |
| 077 ENV-077 | ADDRESSED | The head is a white-edged backboard over a slimmer housing, each lens recessed in a lighter rim under its own hood. | signal |
| 078 ENV-078 | ADDRESSED | The ladder has slim stiles, 22 mm rungs, stand-off brackets to the mast and a landing with hoop guard at head level. | signal |
| 079 ENV-079 | ADDRESSED | The mast has a bolted base flange and access panel; the cabinet has hinges, a lock, louvres and a conduit up the mast. | signal |
| 080 ENV-080 | ADDRESSED | Contact and messenger wires are pitched chords between absolute 12.5 m route points, continuous through curves and chunk seams. | train-side, train-front |
| 081 ENV-081 | ADDRESSED | The grip is a dark cavity inside a proud bevelled rim, shadowed along its top edge and lit along the bottom, so it reads as a recess. | seat-rear-close |
| 082 ENV-082 | ADDRESSED | The pocket is a flush fabric back with an elastic front leaning open to a stitched mouth, side gussets and a leaflet standing in it. | seat-rear-close |
| 083 ENV-083 | ADDRESSED | Posts are collared twelve-sided columns with a darker lower collar and an upper collar under the seat frame. | seat-front-close |
| 084 ENV-084 | ADDRESSED | Base plates are two-step bevelled discs with a central boss and the bolt pair, within the old footprint and contact shadow. | seat-front-close |
| 085 ENV-085 | ADDRESSED | Cases are rounded shells with a seam band, corner guards, a loop handle on posts, a zip line and castors on wheeled cases. | car-5-aisle |
| 086 ENV-086 | ADDRESSED | The leaf sits 15 mm back inside a proud frame with gap lines, three hinge knuckles and a brushed kick plate. | car-1-aisle, car-6-aisle |
| 087 ENV-087 | ADDRESSED | A dark escutcheon carries a lever on a round stem and a lock cylinder, placed below the warning band. | car-1-aisle |
| 088 ENV-088 | ADDRESSED | The extinguisher has a wall strap bracket with base cup, a clipped hose with nozzle and a gauge on the valve. | vestibule, car-6-aisle |
| 089 ENV-089 | ADDRESSED | The intercom has a bezelled housing, a domed red call button in a raised ring, an answer lamp with bezel and EMERGENCY / PRESS AND SPEAK legends. | car-6-aisle |
| 090 ENV-090 | ADDRESSED | Fasteners are 14 mm heads close to the panel tone, seated in a darker countersink ring. | vestibule |
| 091 ENV-091 | ADDRESSED | The bridge plate is a crowned plate with fine lengthwise grooves, hinge lines and yellow nosings instead of raised transverse bars. | gangway |
| 092 ENV-092 | ADDRESSED | Both grab rails end in domed caps above and below their wall mounts. | gangway |
| 093 ENV-093 | ADDRESSED | A shrouded pedestal, oval bowl and stainless cistern cover against the end wall read as a compact railway installation. | toilet |
| 094 ENV-094 | ADDRESSED | The rim, a flat seat ring with hinge block and a raised lid are separate, differently toned parts. | toilet |
| 095 ENV-095 | ADDRESSED | A round flush button in a dark bezel with a label plate sits on the cistern cover above the lid. | toilet |
| 096 ENV-096 | ADDRESSED | The support is a rounded yellow tube with a return and wall brackets. | toilet |
| 097 ENV-097 | ADDRESSED | A rounded dispenser with a visible roll end and hanging tongue sits on the wall beside the bowl. | toilet |
| 098 ENV-098 | ADDRESSED | A 45-degree sanitary cove runs along all four wall/floor junctions below the skirting band. | toilet |
| 099 ENV-099 | ADDRESSED | The basin sits in a counter with a rear upstand over an enclosed cupboard with kick recess and waste flap. | sink |
| 100 ENV-100 | ADDRESSED | A chrome post carries a horizontal sensor spout with a dark sensor window and aerator on a base ring. | sink |
| 101 ENV-101 | ADDRESSED | The mirror shows a vertical gradient from the pale ceiling tone to the teal band with a slanted highlight inside bevelled edges. | sink |
| 102 ENV-102 | ADDRESSED | The soap dispenser has a sight window, push plate and nozzle; a towel dispenser with slot and hanging towel sits beside it. | sink |
| 103 ENV-103 | ADDRESSED | The drain is a chrome strainer ring with a dark outlet. | sink |
| 104 ENV-104 | ADDRESSED | A brushed threshold plate with short ramps and a seal line joins the compartment floor to the vestibule floor. | sink |
| 104b ENV-121 | ADDRESSED | Diffusers are a proud trim frame with corner fasteners around a recessed dark plenum, angled light slats with visible gaps and a centre divider. | hvac-grille-ceiling-before, hvac-grille-ceiling-after |
| 105 ENV-105 | P4 | Own-train side is mostly plain grey horizontal bands without a coherent exterior livery. Design an original service identity. | train-side |
| 106 ENV-106 | P4 | Own-train bogies/wheels are not visible beneath the body, which appears to sit directly on a black strip. Model recognizable running gear. | train-side, train-front |
| 107 ENV-107 | P4 | Own-train underframe has no visible equipment distribution. Add restrained battery/electrical/brake enclosures. | train-side |
| 108 ENV-108 | P4 | Own-train roof is a completely flat bright slab with no curved cross-section. Improve the coach shell silhouette. | train-wide |
| 109 ENV-109 | P4 | Electric train roof has no visible pantograph/electrical collection equipment. Add a plausible fictional arrangement. | train-wide |
| 110 ENV-110 | P4 | Roof lacks visible HVAC housings and service panel divisions. Add a coherent roof equipment kit. | train-wide |
| 111 ENV-111 | P4 | Cab nose is a flat-ended rectangular box with no shaped front transition. Improve its original train design silhouette. | train-front |
| 112 ENV-112 | P4 | Cab face has no readable head/marker light clusters. Add appropriately housed lights. | train-front |
| 113 ENV-113 | P4 | Cab front has no readable coupling/anti-climber assembly. Add plausible external operating hardware. | train-front |
| 114 ENV-114 | P4 | Windscreen has no readable wiper hardware. Add wipers consistent with the glass geometry. | train-front |
| 115 ENV-115 | P4 | Windscreen reads like an open rectangular hole without distinct perimeter seals. Give cab glazing its own closed construction. | train-front |
| 116 ENV-116 | P4 | Inter-car exterior gaps are plain black rectangles instead of flexible bellows. Complete the external gangway connection. | train-side |
| 117 ENV-117 | P4 | External window posts protrude awkwardly against body/header bands. Integrate the aperture into the exterior shell. | window-outside |
| 118 ENV-118 | P4 | Exterior passenger door panels lack a convincing plug-door recess/perimeter seal. Finish the closed-door body joint. | track-close |
| 119 ENV-119 | P4 | Large exterior glazing has almost no edge/lamination cue and shows the opposite interior with implausible clarity. Add subtle edge depth without hiding scenery. | window-outside |
| 120 ENV-120 | P4 | Terminal overview lacks the spatial identity of a terminus: one long shelter beside isolated building blocks and broad blank land. Develop an authored terminal composition. | terminal-wide |

## Verification protocol

For each change record IDs, exact before/after image paths, inspection outcome,
tests, triangle/draw-call deltas and a warmed multi-frame timing comparison when
the change can affect performance. Do not close a whole material/geometry family
because one camera looks better. Keep Low/Medium/High and both train sides viable.
Use existing `build/`, shared system ccache and at most four build workers.
No dependency modifications, other renderer work, or new asset packages are
needed for the first repairs. Next: ENV-001 through ENV-003 window construction,
then ENV-004 through ENV-006 upholstery coverage/material quality.

## Repair evidence

No cosmetic repairs were included in the baseline capture batch. Entries below
will distinguish partial improvements from completed findings.

### Window construction: ENV-001, ENV-002, ENV-003

Implemented a continuous rounded interior flange, recessed reveal, charcoal gasket,
glass-edge return and exterior seal, in the existing opaque interior batch. The
first comparison exposed an additional layout error: structural pillars were
250 mm offset from the shared glazing/seat layout. Pillars and rack brackets now
use `TrainInterior::windowMullionZ`; their width fills the actual inter-pane space.
The end-of-car pier is also closed. Passenger door openings and collision are not
changed. This is a train-simulator construction defect, not a CNA dependency bug.

Inspected matched pairs (same quality, route, pose and three-frame state):

| Subject | Before | After |
| --- | --- | --- |
| Upper corner | [Baseline](../build/visual-audit/window-corner.png) | [Repair](../build/visual-audit/window-corner-after.png) |
| Oblique pane and lower corner | [Baseline](../build/visual-audit/window-oblique.png) | [Repair](../build/visual-audit/window-oblique-after.png) |
| Exterior pane boundary | [Baseline](../build/visual-audit/window-outside.png) | [Repair](../build/visual-audit/window-outside-after.png) |
| Saloon, both sides | [Baseline](../build/visual-audit/car-2-aisle.png) | [Repair](../build/visual-audit/car-2-aisle-after.png) |
| Open doorway | [Baseline](../build/visual-audit/doors-platform.png) | [Repair](../build/visual-audit/doors-platform-after.png) |

Also inspected `car-1-aisle-after` through `car-6-aisle-after`: all six cars use
the same closed assembly; scenery remains unobstructed through the clear pane.
The open-door view retains its original passage. ENV-001/002/003 are addressed
for passenger saloon windows. Cab and door glazing are separate OPEN findings.
Faceted trim shading, flat mullion finish and rectangular fake reflections remain
visible: this is not a claim of photorealistic glazing or completion of ENV-011,
ENV-012, ENV-117 or ENV-119.

`tools/capture_window_repair.sh` reproduces the after set and timing run. Matched
600-frame private-GPU High runs at the corner pose (no avatars, negligible route
advance) measured the following after a 180-frame warmup:

| Metric | Before | After |
| --- | ---: | ---: |
| Draw calls | 88 | 88 |
| Submitted triangles | 667,218 | 691,362 |
| Steady mean frame time | 26.1 ms | 25.7 ms |
| Steady p95 | 26.9 ms | 26.6 ms |
| Steady p99 | 27.2 ms | 27.2 ms |
| Steady maximum | 27.5 ms | 27.7 ms |

Logs: `window-performance-before.log`, `window-performance-after.log` in the
capture directory. The 24,144-triangle increase is approximately 3.6% of this
view's submission, with no new draw calls. Timing variation is not evidence of a
speedup; this single pair shows no material regression and still does **not**
meet a measured 60 FPS target in the private synchronized display configuration.
Geometry tests cover both sides, narrow door-adjacent spans, bounded counts,
nondegenerate triangles, inward normals and shell overlap. Core tests verify every
car's shared pillar-to-pane boundaries. CLI checks reject unbounded audit runs,
NaN coordinates and vertical/singular pitch before renderer startup.

The full existing suite passed **61/61** in 225.88 seconds after this window
repair (`build/visual-audit/window-regression-tests.log`), including interaction,
station exchange/reversal, door interlocks, toilet, renderer quality levels and
asset layout checks. These are automated checks, not a physical mouse/keyboard
play session.

### Complete fabric coverage: ENV-004 and ENV-005

Moved the existing cushion/backrest/crown geometry into the existing upholstery
material batch and mapped all faces at the same seat-local 16 cm repeat. Removed
the separate cushion-top decal; the actual cushion now carries the fabric. The
shaped backrest inset uses matching physical coordinates. Plastic rear shells,
hardware and replaceable pale head covers deliberately retain their own materials.
No downloaded asset, additional texture, new draw call or collision change.

Inspected all four matched pairs from `tools/capture_upholstery_repair.sh`:

| Subject | Before | After |
| --- | --- | --- |
| Cushion fronts and sides | [Before](../build/visual-audit/upholstery-front-before.png) | [After](../build/visual-audit/upholstery-front-after.png) |
| Crown and shoulder | [Before](../build/visual-audit/upholstery-shoulder-before.png) | [After](../build/visual-audit/upholstery-shoulder-after.png) |
| Rear-side fabric versus plastic | [Before](../build/visual-audit/upholstery-rear-before.png) | [After](../build/visual-audit/upholstery-rear-after.png) |
| Normal aisle distance | [Before](../build/visual-audit/upholstery-saloon-before.png) | [After](../build/visual-audit/upholstery-saloon-after.png) |

ENV-004/005 coverage defects are addressed. The pattern is visibly present on the
previously plain cushion fronts, backrest margins and crowns, while rear plastic
panels remain untextured. At this stage **ENV-006 stayed OPEN** pending the subsequent
moquette pass below. Cushion
shape, head-cover folds and rear-shell realism are also not solved by this change.

Matched 600-frame High saloon runs, car 2 / km 52 / no avatars, 419 steady samples:

| Metric | Before | After |
| --- | ---: | ---: |
| Draw calls | 90 | 90 |
| Submitted triangles | 691,746 | 691,382 |
| Carpet + upholstery material triangles | 9,124 | 48,072 |
| Steady mean frame time | 25.8 ms | 26.0 ms |
| Steady p95 | 26.8 ms | 26.9 ms |
| Steady p99 | 27.0 ms | 27.2 ms |
| Steady maximum | 32.8 ms | 27.5 ms |

Logs: `upholstery-performance-before.log` and `upholstery-performance-after.log`.
Material triangles move between existing batches, not duplicate the body; total
submission falls by 364 triangles (182 obsolete two-triangle decals). This is a
single paired frame-pacing observation, not an isolated GPU throughput benchmark.
The new unit regression compares complete body positions, normals and topology to
the previous geometry, checks both seat directions, translation-invariant UVs and
physical top/side mapping. Core and render unit suites passed after this repair.

The final targeted suite passed **12/12** in 34.76 seconds
(`build/visual-audit/upholstery-regression-tests.log`): core/render, interaction,
interior HVAC, both platform door sides, toilet/sink, High material rendering and
capture integrity, Low budget and the avatar-populated showcase. The earlier
61/61 result belongs specifically to the window repair, not a second full run
after the upholstery change. Both dependency worktrees remained clean; builds
used the existing `build/`, `/usr/bin/ccache`, shared `/rv/cnaccache`, and four workers.

### Woven moquette design: ENV-006

Replaced the regularly aligned dark dash grid with a 256-square project-created
material combining 1.25 mm warp/weft, heathered yarn variation, and tapered yarn
flecks with varied positions, lengths and lean. The physical repeat remains 16 cm;
all six car colours still come from the existing vertex tints. Lower-resolution
sources are filtered from the native design, not undersampled independently.

The first sparse variant was rejected after actual engine inspection: it removed
the grid but looked too smooth at ordinary passenger distance. Retained examples
are `moquette-front-rejected.png`, `moquette-shoulder-rejected.png`, and
`moquette-saloon-rejected.png`. A denser motif and stronger fine-yarn contrast
produce the accepted result below. ENV-006's pattern/weave separation is addressed;
this is an albedo approximation, not fibre geometry, normal mapping, or physically
accurate textile scattering. Seat shape and head-cover problems remain OPEN.

| View | Before | Accepted after |
| --- | --- | --- |
| Cushion fronts | [Before](../build/visual-audit/moquette-front-before.png) | [After](../build/visual-audit/moquette-front-after.png) |
| Shoulders and backrest | [Before](../build/visual-audit/moquette-shoulder-before.png) | [After](../build/visual-audit/moquette-shoulder-after.png) |
| Rear fabric margins | [Before](../build/visual-audit/moquette-rear-before.png) | [After](../build/visual-audit/moquette-rear-after.png) |
| Normal aisle distance | [Before](../build/visual-audit/moquette-saloon-before.png) | [After](../build/visual-audit/moquette-saloon-after.png) |

An additional [close weave inspection](../build/visual-audit/moquette-weave-close.png)
uses `--audit-camera 0.58 1.72 -49.5 0 0`, km 52, High, three frames and the same
vibration-free configuration. This is an after-only detail view, not an A/B pair.
The four paired poses use `tools/capture_upholstery_repair.sh before moquette`
and `after moquette`; the script refuses to overwrite its before receipt.

Final matched 600-frame private-GPU saloon measurements (419 steady samples):

| Metric | Before | After |
| --- | ---: | ---: |
| Draw calls | 90 | 90 |
| Submitted triangles | 691,382 | 691,382 |
| Steady mean | 25.5 ms | 25.9 ms |
| Steady p95 | 25.8 ms | 26.6 ms |
| Steady p99 | 26.0 ms | 26.9 ms |
| Steady maximum | 26.7 ms | 27.4 ms |

Logs: `moquette-performance-before.log` / `moquette-performance-after.log`.
This pair is slightly slower, not a claimed speedup; it remains around 26 ms and
does not prove 60 FPS. No draw, shader or geometry changes. The RGBA8 texture plus
full mip chain increases by 256 KiB (128-square to 256-square); no source bitmap
downloads or additional build tree. Tests cover determinism, opaque neutral
colour, contrast, average mip brightness, identical filtered lower resolutions,
and variation under a 4 cm offset.

The final binary passed **10/10** selected tests in 37.67 seconds
(`build/visual-audit/moquette-regression-tests.log`): core/render, all three
quality smoke profiles, interaction, High material rendering and capture integrity,
Low budget and the avatar-populated showcase. CNA and sharp-runtime worktrees
remained unchanged. Next visibility-ranked work is ENV-007 cushion shaping;
another pattern change alone would not address the still box-like furniture.

### Padded cushion profile: ENV-007

Replaced the rounded-plan cuboid with a closed padded profile: a 24 mm central
dish, a 50 mm upper roll, softened underside and a thin continuous side welt.
Removed the three straight top piping bars so they cannot float above the dish.
The 450 × 700 × 200 mm cushion envelope, seat positions, backrests, collision and
interaction behavior remain unchanged. Fabric coordinates unfold continuously
around the roll instead of changing projection axes at the top/side transition.
The welt extends only 0.5 mm beyond the body, not into useful aisle clearance.

Inspected the four pairs from `tools/capture_upholstery_repair.sh before cushion`
and `after cushion`. The front and aisle views show the softer silhouette and
slightly dished top; the shoulder control view confirms unchanged backrests.

| View | Before | After |
| --- | --- | --- |
| Cushion fronts and roll | [Before](../build/visual-audit/cushion-front-before.png) | [After](../build/visual-audit/cushion-front-after.png) |
| Aisle view | [Before](../build/visual-audit/cushion-saloon-before.png) | [After](../build/visual-audit/cushion-saloon-after.png) |
| Rear/side inspection | [Before](../build/visual-audit/cushion-rear-before.png) | [After](../build/visual-audit/cushion-rear-after.png) |
| Backrest control | [Before](../build/visual-audit/cushion-shoulder-before.png) | [After](../build/visual-audit/cushion-shoulder-after.png) |

Also inspected an [avatar-populated seat view](../build/visual-audit/cushion-passenger-after.png),
180 frames, High, car 2, km 52, `--passenger-view`, negligible route time, the same
comparison configuration, no audio. All 18 avatar visuals were ready; seated
passengers remain plausibly supported in this view. This is after-only contextual
validation, not a controlled avatar A/B test or a physical play session.

Each cushion uses 192 triangles. Geometry tests verify welded edge incidence
(exactly two faces per edge), nondegenerate triangles, clockwise winding against
outward normals, unit normals, bounded dimensions, dish/rim/roll presence, both
seat directions and translation-invariant UVs. The final selected suite passed
**10/10 in 35.30 seconds** (`build/visual-audit/cushion-regression-tests.log`):
core/render, interaction, all three quality smoke profiles, High material rendering
and capture integrity, Low budget and the populated showcase.

Matched 600-frame High saloon measurements, 419 steady samples, no avatars:

| Metric | Before | After |
| --- | ---: | ---: |
| Draw calls | 90 | 90 |
| Submitted triangles | 691,382 | 716,862 |
| Steady mean | 26.0 ms | 25.9 ms |
| Steady p95 | 26.6 ms | 26.6 ms |
| Steady p99 | 27.1 ms | 26.9 ms |
| Steady maximum | 27.8 ms | 27.3 ms |

Logs: `cushion-performance-before.log` / `cushion-performance-after.log`.
The net 25,480-triangle increase is about 3.7% for this submission. Existing
material batches and textures are reused; no new draw calls. Timing variation is
not a claimed speedup or proof of 60 FPS. ENV-007 is addressed as bounded visual
geometry, not deformable foam simulation. Faceted headrest crowns (ENV-008), flat
head covers (ENV-009) and rear-shell detail (ENV-010) remain the next priorities.

### Rounded crowns and draped head covers: ENV-008/009

The original twelve-sided, six-band crown had visible angular shoulders. It now
uses 24 sides and ten bands with the same envelope and smooth normals. Replaced
the pale rounded-box plaque with a 380 mm wide cloth surface that follows the
upper crown and wraps slightly behind it. Shallow flutes, a turned lower hem and
smooth surface normals add depth without new textures or draw batches. Seat
collision, interaction points, passenger positions and rear shells are unchanged.
The pale cloth still lacks a dedicated woven texture; this repair addresses its
flat geometry, not full textile realism or simulated cloth motion.

Captured and inspected all four matching before/after pairs with
`tools/capture_upholstery_repair.sh before headrest` and `after headrest`.
An initial flat-normal variant produced visible horizontal shading bands and was
rejected; `headrest-shoulder-rejected.png` and `headrest-saloon-rejected.png`
retain that intermediate evidence. The final version smooths the normals without
increasing its geometry. The low front shot is mainly an unchanged-cushion control.

| View | Before | Accepted after |
| --- | --- | --- |
| Shoulder and cloth wrap | [Before](../build/visual-audit/headrest-shoulder-before.png) | [After](../build/visual-audit/headrest-shoulder-after.png) |
| Saloon and repeated silhouettes | [Before](../build/visual-audit/headrest-saloon-before.png) | [After](../build/visual-audit/headrest-saloon-after.png) |
| Rear shells and distant head covers | [Before](../build/visual-audit/headrest-rear-before.png) | [After](../build/visual-audit/headrest-rear-after.png) |
| Cushion control | [Before](../build/visual-audit/headrest-front-before.png) | [After](../build/visual-audit/headrest-front-after.png) |

The 600-frame paired High-quality run used the same private GPU display, route
position, camera and disabled avatars/audio. Logs and binary receipts use the
`headrest-` prefix in `build/visual-audit/`.

| Metric | Before | After |
| --- | ---: | ---: |
| Draw calls | 90 | 90 |
| Triangles | 716,862 | 840,622 |
| Steady samples | 419 | 419 |
| Steady mean | 26.1 ms | 26.1 ms |
| Steady p95 | 26.9 ms | 26.9 ms |
| Steady p99 | 27.2 ms | 27.3 ms |
| Steady maximum | 27.5 ms | 27.6 ms |

The additional 123,760 triangles (+17.3%) are a real geometry cost across the
182 seats, despite unchanged pacing in this scenario. This is not proof of spare
GPU capacity or 60 FPS; future interior LOD work should include this detail.
There are no new per-frame allocations for these static meshes.

The final build passed **10/10** selected tests in 41.10 seconds, recorded in
`headrest-regression-tests.log`: core/render, all three quality smoke profiles,
interaction, High MSAA, material capture integrity, Low budget and the populated
showcase. New geometry checks cover both seat orientations, bounds, finite
normals, non-degenerate triangles, front/back wrap and crown clearance at vertices
and triangle centres. Existing upholstery translation/UV regressions also pass.
Builds used the existing `build/`, system ccache at `/rv/cnaccache`, and at most
four compiler workers. CNA and sharp-runtime worktrees remained unchanged.

ENV-008/009 are addressed within these visual limits. ENV-010 rear-shell detail
and ENV-011 window reflection artifacts are the next ranked repairs. These
captures and smoke tests are not a long physical mouse/keyboard gameplay test
and do not close the reported intermittent camera-freeze issue.

Also inspected the [populated after-only context](../build/visual-audit/headrest-passenger-after.png)
after 180 frames with `--passenger-view --car 2`, otherwise the same route/config.
All 18 avatar visuals were ready (12 visible); the two nearby seated passengers
remain unobstructed by the revised head covers. This is a context check, not a
matched visual comparison or exhaustive avatar/contact validation.

### Moulded rear seat shells: ENV-010

Replaced the flat slab perimeter with a 14 mm inclined moulding and narrow
return. Added a surround and finger ledge to the dark grab recess. A lighter
blue-grey polymer makes these surfaces readable under cabin lighting; the
existing panel-grain texture supplies restrained fine structure. Shells and
pockets now join the already-rendered interior-panel material batch rather than
adding a new texture or draw call. Seat locations, collision, shell bounds and
the previous headrest/cushion geometry remain unchanged.

Inspected all four matching pairs from `tools/capture_upholstery_repair.sh before
seat-shell` and `after seat-shell`. The close rear view shows the bevel and
recess; aisle/front views show consistency across rows, while the shoulder view
is a control for the unchanged upholstery/window area.

| View | Before | After |
| --- | --- | --- |
| Rear shell close view | [Before](../build/visual-audit/seat-shell-rear-before.png) | [After](../build/visual-audit/seat-shell-rear-after.png) |
| Aisle | [Before](../build/visual-audit/seat-shell-saloon-before.png) | [After](../build/visual-audit/seat-shell-saloon-after.png) |
| Low front | [Before](../build/visual-audit/seat-shell-front-before.png) | [After](../build/visual-audit/seat-shell-front-after.png) |
| Shoulder control | [Before](../build/visual-audit/seat-shell-shoulder-before.png) | [After](../build/visual-audit/seat-shell-shoulder-after.png) |

The initial regression failed because normal-based projection could switch axes
at rounded-edge ties after a seat translation. Fixed this locally with explicit
XY detail projection for the thin shell; other materials retain their existing
mapping. Regression checks now verify translation invariance, the physical UV
scale, outward bevel normals, unit normals, geometry budget and the original
crown/seat-bay clearances in both seat directions.

The paired 600-frame High run used the same private GPU display, route position,
camera and disabled audio/avatars. Binary receipts and logs use the `seat-shell-`
prefix in `build/visual-audit/`.

| Metric | Before | After |
| --- | ---: | ---: |
| Draw calls | 90 | 90 |
| Triangles | 840,622 | 895,950 |
| Steady samples | 419 | 419 |
| Steady mean | 25.9 ms | 26.0 ms |
| Steady p95 | 26.6 ms | 26.9 ms |
| Steady p99 | 26.9 ms | 27.3 ms |
| Steady maximum | 27.1 ms | 27.5 ms |

Geometry increased by 55,328 triangles (+6.6%) across the full consist. The
measurement shows similar frame pacing, not an improvement or a 60 FPS claim.
The original shared ccache and build tree were used with at most four workers;
neither dependency worktree was modified. This is a restrained static plastic
approximation: no normal map, true cavity or interactive rear-seat equipment.
ENV-010 is addressed within those limits; the next ranked defect is ENV-011's
rectangular/diagonal window-reflection artifacts. The physical mouse/keyboard
camera-freeze report remains open and is not tested by these captures.

The final binary passed **10/10** selected tests in 38.99 seconds
(`seat-shell-regression-tests.log`): core/render, all three quality smoke profiles,
interaction, High MSAA, material capture integrity, Low budget and the populated
showcase. The render suite includes the new shell geometry and UV regressions.

### Transparent-edged glass accents: ENV-011

Removed the long hard-edged bright strip and rotated translucent cuboids from
passenger glazing. Replacements are cabin-facing single surfaces with zero alpha
on every perimeter and a restrained central peak. The existing angle-dependent
strength, base tint, frame and transparent rendering batches are retained. This
is a deliberately subtle fake cabin reflection, not an image of the interior.

Inspected matching `glass-reflection-{front,rear,shoulder,saloon}-{before,after}.png`
captures from `capture_upholstery_repair.sh`. The shoulder and aisle pairs show
the distracting rectangular/diagonal overlays gone without obscuring scenery.
The front/rear views confirm no new strong streaks along either window wall.

- [Oblique before](../build/visual-audit/glass-reflection-shoulder-before.png) /
  [after](../build/visual-audit/glass-reflection-shoulder-after.png)
- [Aisle before](../build/visual-audit/glass-reflection-saloon-before.png) /
  [after](../build/visual-audit/glass-reflection-saloon-after.png)

Paired 600-frame High run, 419 steady samples each: draw calls **90 → 90**,
triangles **895,950 → 898,590** (+0.3%), mean **25.6 → 25.5 ms**, p95
**26.4 → 25.8 ms**, p99 **27.0 → 25.9 ms**, maximum **27.2 → 26.2 ms**.
This is comparable frame pacing, not a claimed speedup. Logs and binary receipts
have the `glass-reflection-` prefix. **10/10** selected regression tests passed
in 50.46 seconds (`glass-reflection-tests.log`), including all quality levels,
interaction, material capture and populated showcase. A new geometry test checks
zero perimeter alpha, bounded peak, both-side inward normals and mesh budget.
ENV-011 is addressed; next is ENV-012 passenger-facing mullion trim.

### Finished mullion caps: ENV-012

Retained the structural piers but covered their passenger-facing surfaces with
rounded polymer caps, coordinated with the aperture liners. A 4 mm inset leaves
the surrounding joint legible; narrow end grooves finish the header/sill joins.
Caps reuse the interior-panel texture/batch and stay inside the pier width, not
over the clear glazing. The original passenger-door exclusions are unchanged.

Inspected four before/after pairs from `capture_window_detail.sh`: corner,
oblique, opposite wall and saloon. The previously exposed teal bars now read as
finished cabin trim; seals and rounded window openings remain unobstructed.

- [Corner before](../build/visual-audit/mullion-trim-corner-before.png) /
  [after](../build/visual-audit/mullion-trim-corner-after.png)
- [Opposite wall before](../build/visual-audit/mullion-trim-opposite-before.png) /
  [after](../build/visual-audit/mullion-trim-opposite-after.png)
- [Saloon before](../build/visual-audit/mullion-trim-saloon-before.png) /
  [after](../build/visual-audit/mullion-trim-saloon-after.png)

Paired 600-frame High run, 419 steady samples: **90 → 90** draw calls,
**898,590 → 909,150** triangles (+1.2%), mean **25.7 → 25.9 ms**, p95
**26.6 → 26.7 ms**, p99 **26.9 → 26.9 ms**, maximum **27.0 → 27.0 ms**.
Logs/receipts use `mullion-trim-`. **10/10** selected tests passed in 38.78 seconds
(`mullion-trim-tests.log`), including quality profiles, interaction and populated
showcase. New tests verify both-side bounds, glass clearance, unit normals,
finite UVs and bounded triangle count. ENV-012 is addressed; next is ENV-013.

### Padded armrest profiles: ENV-013

Separated the narrow metal carrier from a softly rolled vinyl pad, retaining the
original 90 × 540 mm footprint and 0.88–0.94 m height. The pad uses two smooth
contour bands and the existing resilient-material texture/batch at a 60 mm repeat.
Support posts, collision and the aisle clearance are unchanged.

Inspected the [front before](../build/visual-audit/armrest-pad-front-before.png) /
[after](../build/visual-audit/armrest-pad-front-after.png) and
[saloon before](../build/visual-audit/armrest-pad-saloon-before.png) /
[after](../build/visual-audit/armrest-pad-saloon-after.png) pairs from the upholstery
capture script. The upper supports no longer read as flat rectangular metal bars.

600-frame High comparison, 419 steady samples: **90 → 90** draws,
**909,150 → 927,006** triangles (+2.0%), mean **26.0 → 26.0 ms**, p95
**26.7 → 26.7 ms**, p99 **27.0 → 26.9 ms**, maximum **27.1 → 27.2 ms**.
Logs and binary receipts use `armrest-pad-`. **10/10** selected tests passed in
39.83 seconds (`armrest-pad-tests.log`), including all quality profiles,
interaction and populated showcase. New geometry tests bound both armrest sides,
unit normals, curved upper normals, UV finiteness and triangle count. This is
static padding, not physical deformation. ENV-013 is addressed.

### Laminate and table edge construction: ENV-014/015

Moved tabletops into the existing polymer-detail batch with a 180 mm material
repeat and retained car-specific colours. Two shaped perimeter bands replace the
hard upper edge. A smaller rounded underside tray replaces the square plate that
previously protruded past the corners. Collision, support posts and authored top
height are unchanged. Cup-holder patches deliberately remain for ENV-016.

Inspected the front and both close side pairs from `capture_table_detail.sh`:
[right before](../build/visual-audit/table-laminate-right-before.png) /
[after](../build/visual-audit/table-laminate-right-after.png),
[left before](../build/visual-audit/table-laminate-left-before.png) /
[after](../build/visual-audit/table-laminate-left-after.png), and
[front before](../build/visual-audit/table-laminate-front-before.png) /
[after](../build/visual-audit/table-laminate-front-after.png). The laminate has
restrained surface variation and the exposed edge/underside has a finished profile.

Paired High 600-frame run, 419 steady samples: **90 → 90** draw calls,
**927,006 → 932,946** triangles (+0.6%), mean **25.4 → 25.8 ms**, p95
**25.6 → 26.5 ms**, p99 **25.8 → 26.8 ms**, maximum **26.0 → 26.9 ms**.
These paced runs do not isolate GPU cost; the observed small increase is recorded,
not described as a performance improvement. Logs/receipts use `table-laminate-`.
**10/10** selected tests passed in 47.42 seconds (`table-laminate-tests.log`).
New tests verify the top envelope, inset tray, bevel normals, finite UVs and mesh
budget. ENV-014/015 are addressed; neither physically based laminate nor a fully
smooth high-poly table is claimed. Next: ENV-016 actual cup recesses.

### Open cup inserts: ENV-016

Removed the grey discs above the table. Partitioned the laminate surface around
two openings, then added a narrow insert lip, tapered inner wall and recessed
floor. The floor is 12 mm below the table, while the rim rises only 1 mm above it.
There is no hidden full tabletop face occluding the openings; support and player
collision remain unchanged. All components reuse the existing table batch.

Inspected [right before](../build/visual-audit/cup-recess-right-before.png) /
[after](../build/visual-audit/cup-recess-right-after.png) and
[left before](../build/visual-audit/cup-recess-left-before.png) /
[after](../build/visual-audit/cup-recess-left-after.png). Both sides now show a rim
and shaded interior instead of a polygon sticker. These are shallow anti-slip
cup wells, not deep bottle holders or interactive drink simulation.

Paired 600-frame High run, 419 steady samples: draws **90 → 90**, triangles
**932,946 → 938,424** (+0.6%), mean **25.9 → 25.5 ms**, p95 **26.5 → 25.8 ms**,
p99 **26.8 → 25.9 ms**, maximum **26.9 → 26.6 ms**. This is comparable pacing,
not an isolated GPU speedup. Logs/receipts use `cup-recess-`. **10/10** selected
tests passed in 39.46 seconds (`cup-recess-tests.log`), including all quality
profiles, interaction and populated showcase. New tests sample nine positions
inside each opening on both table sides to detect covering top triangles; also
check recessed floors, nondegenerate triangles and existing table bounds.
ENV-016 is addressed. Next: ENV-017 ceiling fixture construction.

### Serviceable ceiling-light modules: ENV-017

Replaced the almost-continuous luminous strip with 1.38 m modules on a 1.50 m
pitch. Each has a recessed housing, side rails, end caps and a slightly crowned
opal face with restrained edge falloff. Existing unlit emitters and broad cabin
lighting remain; no extra lighting pass, texture or per-fixture dynamic light.

Inspected the close ceiling, reverse-looking and tunnel before/after pairs from
`capture_ceiling_detail.sh`. The fixture construction and module joints are now
visible; powered faces stay bright in the tunnel, without increasing brightness.

- [Ceiling before](../build/visual-audit/ceiling-fixture-ceiling-before.png) /
  [after](../build/visual-audit/ceiling-fixture-ceiling-after.png)
- [Reverse before](../build/visual-audit/ceiling-fixture-reverse-before.png) /
  [after](../build/visual-audit/ceiling-fixture-reverse-after.png)
- [Tunnel before](../build/visual-audit/ceiling-fixture-tunnel-before.png) /
  [after](../build/visual-audit/ceiling-fixture-tunnel-after.png)

Paired 600-frame High run, 419 steady samples: **90 → 90** draws,
**938,424 → 944,316** triangles (+0.6%), mean **25.6 → 25.9 ms**, p95
**26.2 → 26.6 ms**, p99 **26.5 → 26.9 ms**, maximum **26.8 → 27.2 ms**.
Logs/receipts use `ceiling-fixture-`. **10/10** selected regression tests passed
in 39.68 seconds (`ceiling-fixture-tests.log`). New tests verify module bounds,
downward faces, diffuser colour variation, triangle budget and short-span guards.
ENV-017 is addressed; this is fixture geometry, not photometric lighting.
The new close views also produced ENV-121, the unfinished HVAC grille construction.

### Local furniture grounding: ENV-018

Replaced hard rectangular seat/table floor shadows with soft elliptical falloff
and added tighter seat-pedestal contacts. Outer vertices have zero alpha; the
centres remain bounded and restrained. These static approximations reuse the
existing Medium/High interior-shadow pass. Cabin exposure, light count, Low's
shadow policy and collision are unchanged. This addresses local grounding rather
than claiming physically accurate global illumination or dynamic seat shadows.

Inspected [front before](../build/visual-audit/contact-shading-front-before.png) /
[after](../build/visual-audit/contact-shading-front-after.png) and
[rear before](../build/visual-audit/contact-shading-rear-before.png) /
[after](../build/visual-audit/contact-shading-rear-after.png). The abrupt dark
floor rectangles are gone, with localized contact around the supporting bases.

Final-build 600-frame High comparison, 419 steady samples: **90 → 90** draws,
**944,316 → 958,848** triangles (+1.5%), mean **25.9 → 25.9 ms**, p95
**26.6 → 26.6 ms**, p99 **26.9 → 26.8 ms**, maximum **27.8 → 27.2 ms**.
Logs/receipts use `contact-shading-`. New geometry tests verify transparent outer
edges, bounded footprints, upward normals and nondegenerate triangles.

Ran the entire **61-test** suite: **60 passed**, one High material smoke test
failed solely because it required the previous exact `interior_shadow_triangles=264`.
The actual new count is **14,796**: 48 triangles for each of 93 seat-pair shadows,
182 seat bases and 33 table bases, plus 12 unchanged luggage-shadow triangles.
Updated the exact expectation, with that derivation beside it; did not relax it
to an arbitrary nonzero count. The final build then passed **10/10** selected
tests, including the previously failing test, in **43.10 seconds**. Evidence:
`continuous-campaign-regression.log` (209.20 seconds) and `contact-shading-tests.log`.
The complete 61-test suite was not rerun after this test-configuration correction.

CMake regenerated only the existing build tree; final screenshots and binary
receipts were refreshed after its relink. System ccache and the four-worker cap
were retained. Both dependency worktrees remain clean on `next` (CNA `97783d1e0`,
sharp-runtime `21c06910`). ENV-018 is addressed within the stated approximation.
Next ranked work is ENV-030 car-role differentiation.
CNA and sharp-runtime remain unchanged; builds used the existing tree, shared
system ccache and at most four workers.

### Restrained carpet traffic wear: ENV-019

The runner now has a slightly flattened, desaturated centre with protected darker
edges and modestly stronger wear near each entrance. A bounded vertex-colour grid
preserves the existing woven texture and its 0.85 m physical repeat; no dirt decals,
new texture, render pass or collision change were introduced.

Inspected [front after](../build/visual-audit/carpet-wear-front-after.png) against
[before](../build/visual-audit/carpet-wear-front-before.png), and
[rear before](../build/visual-audit/carpet-wear-rear-before.png) /
[after](../build/visual-audit/carpet-wear-rear-after.png). The effect is intentionally
subtle; the prominent graphic bindings remain separately tracked under ENV-020.

600-frame High comparison (419 steady samples): **90 → 90** draws,
**958,848 → 960,084** triangles, mean **25.9 → 25.9 ms**, p95 **26.6 → 26.6 ms**,
p99 **27.0 → 26.9 ms**, maximum **27.3 → 27.5 ms**. This paced private-display
measurement is not proof of 60 FPS. Logs and binary receipts use `carpet-wear-`.
**10/10** selected tests passed in **39.89 s** (`carpet-wear-tests.log`), including
new geometry checks for wear distribution, UV scale, bounds and degenerate spans.
ENV-019 is addressed.

### Integrated runner bindings: ENV-020

Replaced the 36 mm solid side bars and 24 mm crossbars with 8 mm woven perimeter
tape and 3 mm installation seams, sharing the carpet texture and draw batch.
The tapes follow the actual 780 mm runner boundary, close at both ends, and
meet without overlapping coplanar faces. All details stay within the carpet span,
1 mm above the pile surface; no collision change. The six valid carpet spans have
four edge tapes and nine seams each, so the exact smoke expectation is now 78.

Inspected [front before](../build/visual-audit/aisle-binding-front-before.png) /
[after](../build/visual-audit/aisle-binding-front-after.png), plus
[rear after](../build/visual-audit/aisle-binding-rear-after.png). The distracting
ladder graphic is gone; binding contrast is deliberately low at passenger distance.
600-frame High comparison: **90 → 90** draws, **960,084 → 959,376** triangles,
mean **26.1 → 26.0 ms**, p95 **27.0 → 26.8 ms**, p99 **27.3 → 27.2 ms**,
maximum **27.7 → 27.6 ms** (419 steady samples). Logs/receipts: `aisle-binding-`.
**12/12** selected tests passed in **41.14 s**, including both exact inlay-count
smokes and geometry tests covering end closure, bounds, normals and short spans.
ENV-020 is addressed.

### Rounded suspended grips: ENV-021

Replaced square yellow U bars with closed oval grips, a 28 mm round section,
smooth normals, a narrow metal hanger and distinct connector. Grips share the
existing resilient-material batch at a 60 mm grain repeat. During screenshot
review the inherited gap below the ceiling was also removed: the hanger now
meets a plate at the actual 2.914 m ceiling skin, verified by a geometry test.
The lowest grip point remains above the standing envelope at 2.336 m.

Inspected [reverse before](../build/visual-audit/suspended-handles-reverse-before.png) /
[final after](../build/visual-audit/suspended-handles-reverse-after.png), and
[saloon after](../build/visual-audit/suspended-handles-saloon-after.png). The capture
labelled `close` faces the information screen, so it is not used as grip evidence.
Final 600-frame High comparison: **90 → 90** draws, **959,376 → 984,720** triangles
(+2.6%), mean **25.4 → 25.5 ms**, p95 **25.6 → 25.8 ms**, p99 **25.7 → 25.8 ms**,
maximum **26.2 → 26.1 ms** (419 steady samples). Evidence prefix: `suspended-handles-`.
**10/10** selected tests passed in **41.52 s**. The initial new winding assertion
was incorrect for CNA's clockwise-front convention; corrected it consistently
with existing mesh tests, then verified bounds, finite unit normals, outward
faces and ceiling contact. No dependency modification was needed. ENV-021 is addressed.

### Luggage shelf construction and attachment: ENV-022 / ENV-023

Shelf slats are now six rounded 32 × 16 mm extrusions rather than four 70 × 30 mm
bars. A smaller rounded retaining lip, rear rail and closed end rails complete
each span, including the toilet cut-out. The original 2.230 m luggage support
height and compartment exclusions are preserved. Supports now have a tapered
blade contacting the shelf underside and a rounded two-fastener mounting plate
overlapping the finished structural mullion, replacing blank bulky L blocks.

Inspected [saloon before](../build/visual-audit/rack-construction-saloon-before.png) /
[after](../build/visual-audit/rack-construction-saloon-after.png) and the mounting
detail in [corner before](../build/visual-audit/rack-construction-corner-before.png) /
[after](../build/visual-audit/rack-construction-corner-after.png). The aisle view
shows the lighter shelf sections; the close corner view confirms the plate and bolts.

600-frame High comparison: **90 → 90** draws, **984,720 → 996,624** triangles
(+1.2%), mean **25.5 → 26.1 ms**, p95 **25.7 → 27.0 ms**, p99 **25.7 → 27.4 ms**,
maximum **25.8 → 27.7 ms**. This is a modest measured frame-time increase, not a
performance win; the paced private-display run cannot isolate GPU cost from load.
No new material pass or runtime allocation was added. Evidence: `rack-construction-`.
**10/10** selected regression tests passed in **44.77 s**, including mirrored
bracket winding, shelf/mullion contact, bounds, endpoint and triangle-budget checks.
ENV-022 and ENV-023 are addressed together because their mating heights must agree.

### Passenger information enclosure: ENV-024

The 2.36 m-wide blank slab is now a 1.97 m enclosure with a rounded outer shell,
inset bezel, separate dark face, small fasteners and two ceiling-connected stays.
The dynamic text is width-bounded and its row positions moved inside the face;
previously the footer extended below the screen. Static car-role signage remains
a separate lower panel (its design is tracked under ENV-030).

Inspected [close before](../build/visual-audit/display-housing-close-before.png) /
[after](../build/visual-audit/display-housing-close-after.png). All three live rows
fit the screen and its physical enclosure is readable. Typography remains ENV-025.
600-frame High comparison: **90 → 90** draws, **996,624 → 1,001,640** triangles,
mean **26.1 → 26.0 ms**, p95 **26.9 → 26.7 ms**, p99 **27.3 → 27.0 ms**,
maximum **27.7 → 27.5 ms** (419 steady samples). Evidence prefix: `display-housing-`.
**10/10** selected tests passed in **40.89 s**. New tests verify bounds, both face
depths, unit normals and ceiling contact. ENV-024 is addressed.

### Legible onboard typography: ENV-025

Dynamic screens now use proportionally spaced, antialiased mixed-case DejaVu Sans
with a larger next-station row and subordinate destination/status text. A generated
58 KB embedded coverage/metrics header supplies one 512 × 256 mipmapped atlas;
each visible glyph is two triangles instead of a collection of pixel quads.
Offline generation is reproducible (`generate_display_font.py --check` passed);
no runtime font installation or network is required. License/provenance is in
`ASSETS.md` and the distributed `assets/licenses/DejaVu.txt` notice.

Inspected [close before](../build/visual-audit/display-typography-close-before.png) /
[after](../build/visual-audit/display-typography-close-after.png), the
[aisle view](../build/visual-audit/display-typography-saloon-after.png),
[rear face](../build/visual-audit/display-typography-rear-face.png),
[station dwell](../build/visual-audit/display-typography-station.png) and
[following station](../build/visual-audit/display-typography-following.png).
The supplemental script also exercises reversed service. Static car-role placards,
platform signs and HUD retain their separate font paths; this closes onboard
dynamic information typography, not every sign throughout the world.

Screenshot review caught a project regression missed by the initial 61 passing
tests: restoring `TextureEnabled=false` on the shared effect disabled cloud
coverage on the next frame, producing white cards. A dedicated permanently
textured screen effect now leaves the main effect's identity/cloud textures alone.
This was **not a CNA bug** and no dependency was modified. The retained failing
image is `display-typography-intermediate-state-leak.png`. The new image validator
rejects that image and accepts the final capture; it checks three text rows and
cloud coverage. Both new checks are now in CTest.

Final **63/63** tests passed in **251.70 s**, including interaction, door interlocks,
terminal reversal, station exchanges, material integrity, population, asset and
installation checks (`display-typography-final-tests.log`). Geometry tests cover
atlas alpha/metrics, both face normals, long-line bounds and proportional case.
Final 600-frame High comparison: **90 → 90** draws, **1,001,640 → 984,480** triangles,
mean **26.0 → 25.5 ms**, p95 **26.9 → 25.7 ms**, p99 **27.1 → 25.9 ms**,
maximum **27.2 → 26.4 ms** (419 steady samples). Logs/receipts: `display-typography-`.
ENV-025 is addressed.

### Real passenger-door glazing: ENV-026

The shared opaque door leaf is now built around a real opening, with rounded
flanges, a dark rubber gasket and continuous depth return on both faces. The
separate lightly tinted pane follows the exact animated leaf offset. All 24 panes
share one transparent pass, a persistent scratch mesh and reused streamed buffers;
no geometry upload occurs while the door state is unchanged. Gameplay collision,
onboard-only restrictions, opening side and departure interlocks are unchanged.

Inspected [closed before](../build/visual-audit/door-glazing-closed-right-before.png) /
[after](../build/visual-audit/door-glazing-closed-right-after.png),
[exterior after](../build/visual-audit/door-glazing-exterior-after.png) and
[open vestibule after](../build/visual-audit/door-glazing-vestibule-after.png).
Scenery/interior is visible through the actual openings and panes follow open leaves.
The previously floating control block remains ENV-027, not part of the glazing fix.

600-frame High comparison: **90 → 91** draws, **984,480 → 991,488** triangles,
mean **26.0 → 26.0 ms**, p95 **26.9 → 26.9 ms**, p99 **27.1 → 27.1 ms**,
maximum **27.3 → 27.3 ms** (419 steady samples). Prefix: `door-glazing-`.
**13/13** selected tests passed in **69.27 s**, including moving/departure door
interlocks, both-direction exchange and display/material captures. New geometry
tests cast a 3 × 3 grid through the clear aperture and verify pane bounds, alpha,
two-sided normals and translated positions. ENV-026 is addressed.

### Mounted door controls: ENV-027

Removed the floating coloured blocks from neighbouring windows. Controls now sit
on the actual 130 mm doorway jamb: rounded backing plates, a circular bezel,
separate illuminated face, opening-arrow/two-leaf symbol, English legends and
three raised locating dots (not claimed as Braille). Both interior and exterior
faces are present. Existing released/closing/locked colours remain driven by the
same service state; this does not add manual door operation or permit leaving the train.
The dynamic face reuses the previous control draw, while fixed details join the
interior batch. Close review found window-liner overlap with the first plate
placement; the final mounting depth clears it and has a regression assertion.

Inspected [control before](../build/visual-audit/door-controls-control-right-before.png) /
[after](../build/visual-audit/door-controls-control-right-after.png), the locked
opposite-side capture and [closed-door view](../build/visual-audit/door-controls-closed-right-after.png).
The button is attached to the jamb and the legends are no longer clipped.
600-frame High comparison: **91 → 91** draws, **991,488 → 1,007,328** triangles,
mean **26.0 → 26.1 ms**, p95 **26.8 → 26.8 ms**, p99 **27.1 → 27.0 ms**,
maximum **27.5 → 27.6 ms** (419 steady samples). Prefix: `door-controls-`.
**13/13** selected tests passed in **68.85 s**; new checks cover jamb bounds,
liner clearance, two-sided button normals and face radius. ENV-027 is addressed.

### Continuous folded gangway skin: ENV-028

Replaced nested rigid box ribs with a continuous 12 mm rubber-like skin: seven
rounded corrugations, curved roof corners, closed ends and sill edges, smooth
surface normals and restrained resilient-material grain. Geometry remains within
the former envelope and outside the walking clearance. This is a static visual
approximation, not a newly simulated flexible coupling.

Inspected [side before](../build/visual-audit/gangway-bellows-side-before.png) /
[after](../build/visual-audit/gangway-bellows-side-after.png),
[roof before](../build/visual-audit/gangway-bellows-roof-before.png) /
[after](../build/visual-audit/gangway-bellows-roof-after.png), and the
[entrance](../build/visual-audit/gangway-bellows-entry-after.png). The close roof
view caught a structural header intersecting the new folds; its lower edge now
clears the complete 2.416 m skin at 2.430 m, eliminating black intersection ovals.

600-frame High comparison: **91 → 91** draws, **1,007,328 → 1,019,548** triangles,
mean **26.0 → 25.5 ms**, p95 **26.7 → 25.7 ms**, p99 **27.0 → 25.9 ms**,
maximum **27.5 → 26.0 ms**. Prefix: `gangway-bellows-`. **9/9** selected tests
passed in **59.96 s**, including the collision-path interaction smoke. New mesh
tests require 2,732 triangles per joint, finite unit normals, clockwise winding,
nondegenerate faces, every geometric edge shared exactly twice, corrugation depth
and passenger-clearance bounds. ENV-028 is addressed.

### Finished vestibule bulkheads: ENV-029

Broad near-black fields now have separate matte upper/lower service panels,
rounded edges, narrow dark seals, a horizontal service joint and physical-scale
polymer grain. The formerly bare reverse sides receive matching panels, and thin
header skins stay behind the gangway signs. Safety equipment, the doorway opening
and collision remain unchanged. Surface roughness is represented by the existing
albedo/detail lighting approximation, not a new physically based material model.

Inspected [entry before](../build/visual-audit/vestibule-panels-entry-before.png) /
[after](../build/visual-audit/vestibule-panels-entry-after.png), and
[equipment side before](../build/visual-audit/vestibule-panels-vestibule-before.png) /
[after](../build/visual-audit/vestibule-panels-vestibule-after.png). Service divisions
are legible without adding handles or clutter around the safety fixtures.
600-frame High comparison: **91 → 91** draws, **1,019,548 → 1,023,692** triangles,
mean **25.5 → 25.6 ms**, p95 **25.8 → 26.3 ms**, p99 **25.9 → 26.6 ms**,
maximum **26.0 → 30.8 ms**. The isolated maximum increased and is retained here,
not discarded. Prefix: `vestibule-panels-`. **9/9** selected tests passed in
**43.92 s** after correcting a new test's assumption that rounded-box colours
were unshaded. Bounds, physical UV scale, front/back depths and service separation
are verified. ENV-029 is addressed.

### Car-role cues: ENV-030

Mixed-case role labels now share the passenger-display atlas, with independent quiet,
work, accessible, family, luggage and seating pictograms. Accessible floor markings
stay within the blue side zones. Work tables have charging ports; a small printed
activity book and crayons replace oversized family play blocks and leave both cup
recesses clear. The full-height rack was assigned to the wrong luggage-array index:
it now belongs to Car 5 at z=23 m, with retaining straps and a thin rear panel that
does not intersect the upper cases. Seat counts, collision and NPC ownership are unchanged.

Inspected all six `car-roles-car-N-before/after.png` engine views plus
[family before](../build/visual-audit/car-roles-family-before.png) /
[after](../build/visual-audit/car-roles-family-after.png),
[accessible before](../build/visual-audit/car-roles-accessible-before.png) /
[after](../build/visual-audit/car-roles-accessible-after.png),
[work before](../build/visual-audit/car-roles-work-before.png) /
[after](../build/visual-audit/car-roles-work-after.png), and
[rack before](../build/visual-audit/car-roles-rack-before.png) /
[after](../build/visual-audit/car-roles-rack-after.png). The original luggage close
pose pointed at a table, so the rack evidence is a supplemental pair captured
before/after its relocation. Near-cab views in cars 1/6 do not show the role boards.
Capture script: `tools/capture_car_roles.sh`.

600-frame High comparison: **91 → 91** draws, **1,023,692 → 1,017,400** triangles;
steady mean **25.7 → 25.5 ms**, p95 **26.4 → 25.8 ms**, p99 **26.7 → 25.9 ms**,
maximum **27.3 → 26.0 ms**. Private-compositor timings are not a 60 FPS certification.
Role/icon bounds and book/table clearance tests pass. The full run passed **62/63**
tests; terminal reversal reached the correct direction but its 120-frame window
ended with two exchange actors still active. Its real-time traversal needs a longer
test window on faster rendering. Final acceptance waits for the unchanged assertions
to pass with sufficient frames; see `car-roles-full-tests.log` for the original failure.
The final full suite passed **63/63 in 97.83 s** with the 600-frame reversal check,
including the subsequent trunk repair. No assertions were removed or relaxed.
ENV-030 is addressed.

### Continuous tree trunks: ENV-031

The production trunk helper scaled its radius but not height, while its branch
roots and crowns used full tree scale. Large trees therefore had detached limbs;
saplings could have overlong trunks. A shared three-ring taper now scales every
axis, closes both caps and eliminates the overlap/step between two cylinders.
All five trunk call sites use the same helper, with their original authored
heights and LOD side counts. The renderer remains in the existing opaque batch.

Inspected [urban before](../build/visual-audit/tree-continuity-urban-before.png) /
[after](../build/visual-audit/tree-continuity-urban-after.png), country, oblique
window and lake pairs under the same `tree-continuity-` prefix. The urban right
tree's disconnected branch root is joined; nearby saplings no longer carry
unscaled poles. This does not close the separate crown-porosity finding ENV-032.
600-frame High: **91 draws and 1,017,400 triangles unchanged**; steady mean
**25.5 → 25.5 ms**, p95 **25.7 → 25.8 ms**, p99 **25.9 → 25.9 ms**,
maximum **26.4 → 26.1 ms**. Unit tests cover six scales from 0.25 to 4, all four
production side counts, uniform position scaling, top/branch bounds, finite unit
normals and exactly two triangles sharing each geometric edge. Full **63/63**
passed in **97.83 s** (`tree-continuity-full-tests.log`). ENV-031 is addressed.

### Porous broadleaf crowns: ENV-032

Medium/High near crowns retain small opaque inner masses at 60% lobe radius;
three crossed, double-sided alpha-tested sprays per lobe now define the outer
silhouette. The old full-radius opaque lobes hid the leaf cutouts. Sprays are
world-oriented, not camera-facing, use the existing coverage-aware texture/mips
and share the existing batch. Low and distant LODs remain deliberately simplified.
The initial sparse/bright trial was adjusted after viewing its urban/country
frames; final foliage has a less conspicuous tint discontinuity.

Inspected [country before](../build/visual-audit/broadleaf-porosity-country-before.png) /
[after](../build/visual-audit/broadleaf-porosity-country-after.png),
[urban before](../build/visual-audit/broadleaf-porosity-urban-before.png) /
[after](../build/visual-audit/broadleaf-porosity-urban-after.png), lake and oblique
window views. Sky/buildings are now visible through crown gaps and leaf edges
interrupt the solid ball silhouette. These are still compact hybrid game trees,
not individually modelled leaves; static frames do not establish temporal shimmer.
Capture: `tools/capture_tree_continuity.sh before|after broadleaf-porosity`.
600-frame High: **91 draws unchanged**, **1,017,400 → 1,027,600** triangles (+1.0%);
steady mean **25.9 → 26.0 ms**, p95 **26.6 → 26.6 ms**, p99 **27.0 → 26.9 ms**,
maximum **27.5 → 27.1 ms**. **12/12** selected tests passed in **12.25 s**, including
all three foliage quality paths, core-radius bounds, deterministic geometry,
crossed-card bounds and mip coverage. ENV-032 is addressed.

### Conifer needle silhouettes: ENV-033

Medium/High near conifers replace broad opaque branch fans with slender supports
and three crossed cutout fronds per branch. A project-generated 128-square needle
motif has irregular twig reach/spacing, transparent gaps and coverage-aware mips.
A reduced central mass keeps depth between whorls. One shared needle-cutout batch
uses CNA's existing alpha-test effect, setting its own texture explicitly before
each leaf/needle draw. Low and distant trees retain their cheaper geometry.

Inspected [country before](../build/visual-audit/conifer-needles-country-before.png) /
[after](../build/visual-audit/conifer-needles-country-after.png) and lake pairs;
urban/oblique captures check that the shared effect does not alter broadleaf trees.
The first sparse trial was rejected in favour of denser, irregular needle cover.
Edges now have feathered cutouts instead of continuous triangular skirt faces.
Whorls remain a bounded approximation without wind animation, and distant Low
silhouettes are still angular. Prefix: `conifer-needles-`.
600-frame High: **91 → 92** draws, **1,027,600 → 1,072,330** triangles (+4.4%);
steady mean **26.0 → 25.5 ms**, p95 **26.7 → 25.7 ms**, p99 **27.0 → 25.8 ms**,
maximum **27.1 → 26.1 ms**. This is measured variability, not an optimization claim.
**12/12** selected tests passed in **15.80 s**, including three quality paths,
frond/core budgets, bounds, normals, texture repeatability and transparent borders.
ENV-033 is addressed.

### Rounded leafy shrubs: ENV-034

Medium/High route bushes and tree understorey use three smooth, irregular inner
lobes with crossed leaf-cutout edges in the existing foliage batches. Upright
cards are clipped to ground height, and standalone bushes use soft contact
shadows instead of a solid rectangular footprint. The first trial was too small;
final lobe dimensions restore substantial presence rather than hiding the issue.
Low retains its inexpensive faceted approximation.

Inspected [country before](../build/visual-audit/shrub-foliage-country-before.png) /
[after](../build/visual-audit/shrub-foliage-country-after.png), urban and lake views.
Flat-sided green blocks are now rounded leafy groups; these passenger-distance
frames do not establish botanical detail at extreme close range.
600-frame High: **92 draws unchanged**, **1,072,330 → 1,128,568** triangles (+5.2%);
steady mean **25.9 → 25.7 ms**, p95 **26.6 → 26.4 ms**, p99 **26.9 → 26.9 ms**,
maximum **27.4 → 34.9 ms**. The maximum spike increased despite stable percentiles.
**12/12** selected tests passed in **12.81 s**, including all foliage-quality
paths and new tests for bounds, ground clipping, deterministic geometry and the
162-triangle shrub budget. Prefix: `shrub-foliage-`. ENV-034 is addressed.

### Leafy hedge profiles and ends: ENV-035

Medium/High hedges now have rounded smoothly lit profiles, strongly tapered ends,
overlapping irregular leaf masses and cutout side/crown/end fringes in the shared
foliage batches. Physical-scale detail replaces flat untextured side walls.
Ground samples still follow the terrain; Low retains the cheaper original profile.
An early texture-only trial still read as an extrusion, so the final repair also
changes the body and end geometry. End-fringe height was corrected after its new
ground-clearance test found an intersection.

Inspected [country before](../build/visual-audit/hedge-foliage-country-before.png) /
[after](../build/visual-audit/hedge-foliage-country-after.png) and oblique window
views. Ends and exposed sides are rounded planted forms, although the bounded
1.5 m span cadence remains an approximation rather than individually authored shrubs.
600-frame High: **92 draws unchanged**, **1,128,568 → 1,208,716** triangles (+7.1%);
steady mean **25.9 → 25.5 ms**, p95 **26.6 → 25.7 ms**, p99 **27.0 → 25.8 ms**,
maximum **27.2 → 25.9 ms**. The geometry cost is explicit; these timings do not
establish an optimization or guaranteed 60 FPS.

Final **63/63** tests passed in **101.56 s** (`hedge-foliage-final-tests.log`). An
earlier full run exposed another pre-existing timing assumption: welcome playback
waits 0.10 s plus a 0.92 s chime, exceeding its 60-frame test at 60 Hz. Both welcome
tests now run 120 frames with the same assertions; the original reverse failure
is retained in `hedge-foliage-full-tests.log`. New hedge tests cover taper, slope
following, cutout bounds and finite normals. ENV-035 is addressed.

### Clustered verge grass: ENV-036

Medium/High verge placement now groups narrower, shorter bent blades into three
small clumps per absolute-route slot, instead of spreading isolated broad spikes
across the slot. High uses 15 blades every 3 m, Medium nine every 6 m; Low retains
three every 50 m on alternating banks. Standalone grass patches also form compact
radial groups. Brighter tips and shaded bases improve blade separation without
textures, transparency or additional draws. Terrain/station/water exclusions remain.

Inspected [country before](../build/visual-audit/grass-clusters-country-before.png) /
[after](../build/visual-audit/grass-clusters-country-after.png), with urban/lake
captures retained to check exclusions. This is low verge grass, not full meadow
coverage or simulated wind. 600-frame High: **92 draws unchanged**,
**1,208,716 → 1,286,698** triangles (+6.5%); steady mean **25.5 → 25.9 ms**,
p95 **25.7 → 26.5 ms**, p99 **25.8 → 26.9 ms**, maximum **25.9 → 29.4 ms**.
Initial world generation/upload measured **78.4/55.1 → 67.4/51.9 ms**, but frozen
route timing alone does not establish smooth moving-world streaming.
**12/12** selected tests passed in **13.40 s**; added tests cover every quality,
both banks, route endpoints, deterministic clustered positions and bounded widths,
heights and wrapped yaw. Prefix: `grass-clusters-`. ENV-036 is addressed.

### Rural field margins: ENV-037

Medium/High rural terrain now blends parcel soil through three-metre grass margins
at longitudinal and lateral boundaries, with restrained deterministic soil variation.
Subdivided cells preserve the original terrain diagonal and height exactly; no
coplanar overlay or new material submission is introduced. Low retains its original
terrain budget. Authored crop patches and vegetation placement are unchanged.

Inspected [river before](../build/visual-audit/field-margins-river-before.png) /
[after](../build/visual-audit/field-margins-river-after.png), viaduct and country
views. Broad parcels remain deliberately agricultural, but their hard colour edges
now have planted transitions. This is not crop growth or fully authored field shapes.
600-frame High: **92 draws unchanged**, **1,286,698 → 1,292,298** triangles (+0.44%);
steady mean **26.1 → 26.0 ms**, p95 **26.9 → 26.6 ms**, p99 **27.1 → 26.8 ms**,
maximum **27.4 → 27.2 ms**. These frozen-route timings do not resolve moving-world
streaming hitches. **12/12** selected tests passed in **12.96 s**. New tests verify
boundary colour continuity, unchanged corridor/far terrain, deterministic variation,
and subdivision vertices/triangle centroids against the original nonplanar surface.
Prefix: `field-margins-`, with final binary receipt. ENV-037 is addressed.

### Woodland grouping: ENV-038

Rural near and middle-distance trees now use deterministic five-tree copses with
irregular centres, radii and bank selection. Occasional isolated trees remain;
urban planting is unchanged. The small distant renderer band is also grouped.
Candidate counts and random-number consumption are unchanged, preserving later
building/landmark generation. Existing crop, water and corridor exclusions still
apply. Existing understorey follows the relocated near trees. This is a lightweight
habitat pattern, not a forest ecology simulation or additional forest density.

Inspected [river before](../build/visual-audit/woodland-groups-river-before.png) /
[after](../build/visual-audit/woodland-groups-river-after.png), viaduct and country
views. Visible copses alternate with clearings rather than evenly dotted ground.
600-frame High: **92 draws unchanged**, **1,292,298 → 1,302,938** triangles (+0.82%);
placement affects distance LOD and crop rejection despite unchanged candidate budgets.
Steady mean **25.5 → 25.5 ms**, p95 **25.8 → 25.7 ms**, p99 **25.9 → 25.8 ms**,
maximum **26.1 → 25.9 ms**. Frozen-route timings do not close streaming hitches.
Final **12/12** selected tests passed in **47.00 s**. The new three-location test
checks tree budgets, majority grouping, terrain exclusions, chunk containment and
exact regeneration after unloading. Prefix: `woodland-groups-`. ENV-038 is addressed.

### Grounded hill silhouettes: ENV-039

Replaced flattened ellipsoids with asymmetric height-field mounds. Each perimeter
vertex samples the rendered terrain and is buried 30 cm below it; there is no
lower hemisphere or exposed lens rim. Finite-difference normals smooth the slopes,
while seeded overlapping lobes vary the silhouette. Low/Medium/High use bounded
4/8/12 grids and retain the original placement/count and atmospheric colour bands.
These remain distant approximations, not an authored mountain landscape; skirts
sample the terrain rather than clipping every triangle against it.

Inspected [country before](../build/visual-audit/grounded-hills-country-before.png) /
[after](../build/visual-audit/grounded-hills-country-after.png), river and viaduct.
The formerly detached underside is absent in these views. **12/12** selected tests
passed in **45.71 s**, including new three-quality slope, skirt-height, normal and
triangle-budget checks. 600-frame High: **92 draws unchanged**, **1,302,938 →
1,306,522** triangles (+0.28%); steady mean **26.0 → 26.0 ms**, p95 **26.8 →
26.8 ms**, p99 **27.2 → 27.3 ms**, maximum **27.7 → 27.9 ms**. Initial world
generation **81.5 → 74.0 ms** is a noisy single rebuild, not an optimization claim.
Prefix: `grounded-hills-`. ENV-039 is addressed.

### Sunny cloud composition: ENV-040

One four-tile atlas replaces the single repeated silhouette: rounded cumulus,
asymmetric taller cumulus, thin horizontal bank and ragged small cluster. Each has
its own lobe profile and density seed. Elevation and aspect ratio vary while the
original 4/6/8 billboard quality budgets remain. This stays static sunny daytime,
not weather simulation. The 512-square atlas adds approximately 1 MiB of mipmapped
RGBA storage over the old 256-square texture, with no extra texture binding or draw.

Inspected [river before](../build/visual-audit/cloud-variety-river-before.png) /
[after](../build/visual-audit/cloud-variety-river-after.png), lake, country and oblique
views. **92 draws and 1,306,522 triangles unchanged**; 600-frame High steady mean
**26.1 → 26.0 ms**, p95 **26.9 → 26.8 ms**, p99 **27.3 → 27.2 ms**, maximum
**27.6 → 27.2 ms**. Atlas tests verify distinct alpha silhouettes, exact tile copies
and transparent borders; the original cloud-material tests remain.

The full run was **61/63**, not a clean pass: the display validator sampled the old
cloud elevation, and reversal hit a 30-second software-render timeout. Its updated
sample rectangle was verified against the actual engine image and still rejects a
uniform white card. Retrying reversal exposed the deeper frame-count assumption:
fixed-step catch-up runs multiple updates per draw, so 600 software-rendered frames
had already completed three station legs. New `--run-seconds 10` uses unscaled,
unpaused update time, retaining all original reversal/state assertions. The corrected
reversal and display tests both passed (**2/2, 11.15 s**), with failing logs retained.
Prefix: `cloud-variety-`; corrective evidence: `duration-limit-tests.log`.
ENV-040 is addressed; a complete post-correction suite is still required.

### Connected urban frontage: ENV-041

Existing townhouse candidates now form paired blocks with a consistent pavement
setback, shared paved passages and street-edge masonry garden boundaries. Entrances,
service access, crossings and public courts remain open. Passage footprints are
included in vegetation exclusion. Candidate counts are unchanged; yard/substation
conflicts and overlap rejection can reduce actual counts. The initial centred-bin
layout left empty streets at every chunk seam; end blocks now sit nearer the seams.
This establishes a connected foreground street, not a complete city street network;
the lower-density rear layer and entrance detailing still have separate audit work.

Inspected both banks at km 2 and 191, plus the unchanged lake context. See
[city before](../build/visual-audit/urban-frontage-city-left-before.png) /
[after](../build/visual-audit/urban-frontage-city-left-after.png).
600-frame High city benchmark: **111 draws unchanged**, **1,329,854 → 1,316,246**
triangles; steady mean **25.6 → 26.0 ms**, p95 **26.3 → 26.8 ms**, p99 **26.7 →
27.1 ms**, maximum **27.1 → 27.4 ms**. These are small timing differences, not a
60 FPS claim. New tests cover paired spacing, plot-link bounds, door/crossing
clearance and vegetation exclusion; final core run passed in **2.74 s**.

Full suite: **61/63, 228.92 s**. Reversal and display now passed; only the two
optional facade-effect image comparisons failed. Raw major-error fractions were
0.001087/0.001111 against a 0.001 threshold after visible frontage edges changed.
Actual stock/control images were inspected. One-pixel-neighbour residuals were
377/355 interior pixels, while mean colour errors remained 0.1265/0.1632 and the
linear-light wall change remained 10.16%. The test now retains the same mean and
linear-change guards, bounds raw error at 0.002 and adds a stricter interior guard
of 0.0005. Both corrected comparisons passed (**2/2, 18.45 s**); original failures
are retained. A fresh whole-suite pass is still pending after this correction.
Prefix: `urban-frontage-`. ENV-041 is addressed.

### Residential glazing and room variation: ENV-042

Near Medium/High house, townhouse and apartment windows now have graded dark-to-sky
glass and four restrained room treatments, including partial blinds and side curtains.
Both facade orientations retain the existing raised frame and mullions, with cues
bounded at 67.5–69 mm versus the frame's 110 mm front. Absolute feature distance,
variant, floor and column select the pattern, so rebasing cannot change a room.
Low/distant panes remain simpler. These are opaque room cues, not transparent
modeled rooms or physical reflections; no additional sorting/material pass is needed.

Inspected [close before](../build/visual-audit/residential-glass-facade-close-before.png) /
[after](../build/visual-audit/residential-glass-facade-close-after.png), both passenger
banks, lake and oblique views. Supplemental close baselines were captured from the
same pre-repair binary before compiling the change. Geometry tests cover all four
patterns, both facing normals, frame-depth bounds, repeatability and an 8–16 triangle
budget per detailed pane. Initial core/render tests: **2/2, 2.97 s**.

The complete post-correction suite is now green: **63/63, 316.14 s** in
`residential-glass-full-tests.log`, including reversal, both facade-effect comparisons,
display validation, six-car interaction smoke, doors, station exchange and installation.
This supersedes the pending whole-suite checks recorded in ENV-040/041, without
erasing their failed runs. 600-frame High city: **111 draws unchanged**, **1,316,246 →
1,319,010** triangles (+0.21%); steady mean **25.9 → 26.0 ms**, p95 **26.5 →
26.9 ms**, p99 **26.7 → 27.3 ms**, maximum **27.0 → 27.8 ms**.
Prefix: `residential-glass-`. ENV-042 is addressed.

### Roof surfaces and fittings: ENV-043

Near Medium/High townhouse and apartment roofs now have a fine mineral-surfaced
membrane, bounded roll joints, coping, two small drains and one louvred vent on a
curb. They reuse the existing neutral terrain-detail batch and opaque geometry;
fittings are omitted beyond 450 m and on Low. Existing pitched tile scale remains
0.30 × 0.40 m, with hashed per-tile colour variation replacing regular diagonal
variation. A stronger first trial read as patterned stripes in the close screenshot;
the final modulation was reduced. Gutters and house drainage remain ENV-044.

Inspected [flat roofs before](../build/visual-audit/roof-materials-roof-oblique-before.png) /
[after](../build/visual-audit/roof-materials-roof-oblique-after.png), pitched roof
close-ups and normal passenger views. **12/12** selected tests passed in **75.46 s**;
after the final colour-only adjustment the render tests passed again. New geometry
tests cover the surface/coping envelope, finite normals, height above the old slab,
bounded geometry and invalid dimensions. 600-frame High city: **111 draws unchanged**,
**1,319,010 → 1,328,510** triangles (+0.72%); steady mean **25.4 → 25.9 ms**,
p95 **25.7 → 26.9 ms**, p99 **25.7 → 27.3 ms**, maximum **26.0 → 27.5 ms**.
The timing increase is retained, not presented as a performance improvement.
Prefix: `roof-materials-`. ENV-043 is addressed.

### Closed eaves and rainwater goods: ENV-044

All pitched roofs now have a closed soffit, including Low quality: the close
baseline revealed a real sky gap under the overhang, not just missing trim.
Near Medium/High houses additionally have eight-segment open gutter troughs,
end caps, supports and connected downpipes with offset bends, clamps and outlets.
Existing opaque batches are reused. Distant roofs retain simpler gutters.

Inspected [under-eave before](../build/visual-audit/eave-drainage-eave-close-before.png) /
[after](../build/visual-audit/eave-drainage-eave-close-after.png) and elevated roof
views. The gap is closed and both downpipes join the eaves. Geometry tests cover
soffit closure, pipe outward winding, valid normals, two outlets, bounds and
invalid inputs. Final selected tests: **12/12, 45.86 s**.
600-frame High city: **111 draws unchanged**, **1,328,510 → 1,347,018** triangles
(+1.39%); steady mean **26.0 → 25.9 ms**, p95 **26.6 → 26.7 ms**, p99
**26.9 → 27.0 ms**, maximum **27.5 → 27.3 ms**. This compositor-paced run is
not a 60 FPS claim. Prefix: `eave-drainage-`. ENV-044 is addressed.

### Residential entrance construction: ENV-045

Near Medium/High house, townhouse and apartment entrances now have raised frames,
panel borders, lever handles, thresholds and domestic mail flaps or a communal
upper light. Both facade orientations use the same opaque batch, with a 250 m
detail limit. New close views also exposed existing house and apartment windows
overlapping the door head/edge; those ground-floor window bays are now reserved
for the entrance at every quality.

Inspected [house before](../build/visual-audit/entrance-doors-house-close-before.png) /
[after](../build/visual-audit/entrance-doors-house-close-after.png), townhouse and
background apartment views. The second capture prompted the apartment clearance
correction; final images confirm it. Tests cover both-facing hardware, frame and
threshold bounds, normals, invalid dimensions and the 192–216 triangle kit budget.
Final selected tests: **12/12, 14.51 s**. 600-frame city: **111 draws unchanged**,
**1,347,018 → 1,349,170** triangles (+0.16%); steady mean **25.7 → 25.9 ms**,
p95 **26.5 → 26.8 ms**, p99 **27.0 → 27.1 ms**, maximum **27.2 → 27.3 ms**.
No claim of interactable exterior buildings: the player remains aboard.
Prefix: `entrance-doors-`. ENV-045 is addressed.

### Street materials and edge cross-sections: ENV-046

Medium/High streets reuse neutral detail for asphalt grain and the existing concrete
material for footways and chamfered precast kerbs. Both follow identical curved
route samples. Outer footway width now closes the former 180 mm gap to the kerb.
Thin 650 × 650/700 mm slab joints are limited to 250 m; Low retains simpler strips.
No new textures or per-object material resources were added.

Inspected [street before](../build/visual-audit/street-surfaces-townhouse-close-before.png) /
[after](../build/visual-audit/street-surfaces-townhouse-close-after.png) and both-bank
passenger views. Geometry tests verify grade-following bounds, chamfer/top normals,
closed end normals, invalid spans and an 18-triangle section budget. Selected tests:
**12/12, 12.88 s**. 600-frame High city: **111 → 112 draws** (the concrete pass is
now present away from stations), **1,349,170 → 1,377,034** triangles (+2.07%);
steady mean **25.4 → 26.0 ms**, p95 **25.6 → 26.6 ms**, p99 **26.0 → 26.9 ms**,
maximum **26.6 → 28.4 ms**. The measured increase is retained; this is not a
performance improvement or a 60 FPS result. Prefix: `street-surfaces-`.
ENV-046 is addressed. The complete regression suite subsequently passed **63/63,
88.37 s**, including facade comparisons, service reversal and installation, in
`street-surfaces-full-tests.log`.

### Landscaped residential access corridors: ENV-047

Building approaches retain the connected pavement/parcel layout from ENV-041,
with concrete grain, low edging and six modest planted pockets beside long paths
within 350 m on Medium/High. A first capture showed undersized planting, which
was enlarged before acceptance. Open lawns remain deliberately present; this does
not claim that the wider city is fully landscaped. Door offsets and access widths
also define deterministic vegetation exclusions, including raw neighbouring chunks.

Inspected [oblique before](../build/visual-audit/access-gardens-access-oblique-before.png) /
[after](../build/visual-audit/access-gardens-access-oblique-after.png) and passenger
street views. Tests verify both-bank clear walking widths, planting bounds, access
offsets and whole-route tree-crown exclusion. Final selected tests: **12/12, 13.10 s**.
600-frame High city: **112 draws unchanged**, **1,377,034 → 1,386,626** triangles
(+0.70%); steady mean **26.0 → 26.1 ms**, p95 **26.8 → 26.9 ms**, p99
**27.0 → 27.2 ms**, maximum **27.2 → 27.3 ms**. No new material resources.
Prefix: `access-gardens-`. ENV-047 is addressed.

### Road-car construction: ENV-048

All six cached colour variants now use a tapered bonnet/body, real wheel-arch
cutouts with inset dark liners, sky-graded opaque glazing, door seams and handles,
front grille, blank fictional plates and five-spoke hubs. Each car remains one
draw. The first two close captures exposed bright pavement through unlined arch
depth; the final version closes that reveal, not just its rear plane.

Inspected [close before](../build/visual-audit/road-cars-road-close-before.png) /
[after](../build/visual-audit/road-cars-road-close-after.png) and the passenger view.
Traffic position can advance slightly during startup; camera and route are matched.
Tests cover wheel clearance, body bounds, unit normals, hub outward facing and
bounded mesh counts. Final selected tests: **12/12, 13.17 s**.
600-frame High city: **112 draws unchanged**, **1,386,626 → 1,395,086** triangles
(+0.61%); steady mean **26.1 → 26.1 ms**, p95 **26.8 → 27.0 ms**, p99
**27.2 → 27.2 ms**, maximum **27.5 → 27.9 ms**. No traffic-AI changes are claimed.
Prefix: `road-cars-`. ENV-048 is addressed.

### Flat station typography: ENV-049

Station names, platform identifiers and live departure-board text now reuse the
licensed onboard coverage atlas. Each visible glyph is one flat quad, with actual
font ascent used for top anchoring and inward-facing placement on both banks.
Static names stream with the world; live text keeps its existing service-state key.
Two shared alpha passes replace thousands of tiny opaque pixel-box triangles.
The dedicated font effect and restored depth/blend state avoid changing scenery
materials. Dynamic faces sit 10 mm ahead of the board surface, not coplanar.

Inspected city, rural, terminal and waterfront before/after frames, including
[city before](../build/visual-audit/station-type-city-before.png) /
[after](../build/visual-audit/station-type-city-after.png). Oversized signs and
remaining sightline occlusion are not closed here; those are ENV-050/061.
Tests cover flatness, inward normals, glyph order, atlas bounds and invalid width.
Final selected tests: **12/12, 13.44 s**. Final 600-frame station: **116 → 118
draws**, **1,310,506 → 1,298,228** triangles (−0.94%); steady mean **25.4 →
26.0 ms**, p95 **25.7 → 26.9 ms**, p99 **25.8 → 27.1 ms**, maximum
**26.1 → 27.3 ms**. Prefix: `station-type-`. ENV-049 is addressed.

### Compact supported station signs: ENV-050

Name boards are now 2.80 × 0.48 m instead of 3.80 × 0.90 m, with a thin footer,
two grounded supports and base plates. Three boards serve each platform. The end
boards use the rear edge; the centre board is 0.40 m farther from the train than
before. Smaller suspended platform identifiers move clear of the live board.
The first rearward centre placement was not accepted: a turned seated view hit
the headrest. Subsequent straight alignment also needed entrance clearance. Final
centre offset −8.5 m preserves the public doorway and readable window angle.

Inspected all matched platform views and supplemental 1.90 m seated-height views
on both banks. [Before](../build/visual-audit/station-sign-layout-city-before.png) /
[final after](../build/visual-audit/station-sign-layout-city-after.png).
Final refinement was validated together with ENV-051; those after frames also
contain the facade kit. Tests cover supported geometry bounds, platform fixture
clearance and all small/major entrance-offset variants. Whole-suite check on the
earlier layout: **63/63, 88.64 s**; final combined selected check: **12/12, 13.23 s**.
600-frame station, including the subsequent facade kit: **118 draws unchanged**,
**1,298,228 → 1,298,572** triangles (+0.03%); steady mean **25.5 → 26.0 ms**,
p95 **25.8 → 26.9 ms**, p99 **25.8 → 27.3 ms**, maximum **26.3 → 27.6 ms**.
Prefix: `station-sign-layout-`. ENV-050 is addressed.

### Station facade families: ENV-051

Rural and waterfront waiting halls now use different painted boarding palettes,
narrower/wider windows and a small supported entrance hood. Urban and suburban
families have vertical piers, a lintel band, metal-toned framing and different bay
counts; suburban plinths are taller. Existing terminal hall massing is retained.
Boarding stays behind the window beads and excludes the entrance opening.
Door clearance now accounts for the selected window width rather than one fixed
window size. Low retains simpler facade geometry but the new proportions.

Inspected [rural before](../build/visual-audit/station-families-rural-before.png) /
[after](../build/visual-audit/station-families-rural-after.png), the matched
Stonebridge view and supplemental major-city/waterfront facade views. The original
`city` capture label denotes Stonebridge, not a `major` station; Northbridge was
therefore additionally inspected after implementation, without claiming a matched
major-city baseline. Window-glass realism and further surface finish remain
ENV-058/059, not silently closed here.

Final selected checks: **12/12, 13.23 s**, including all four facade families,
entrance clearance, finite unit normals and bounded geometry. 600-frame station:
**118 draws unchanged**, **1,298,476 → 1,298,572** triangles; steady mean **26.1 →
26.1 ms**, p95 **26.9 → 26.8 ms**, p99 **27.2 → 27.1 ms**, maximum **27.8 →
27.4 ms**. Prefix: `station-families-`. ENV-051 is addressed.

### Supported canopy panels and display clearance: ENV-052, ENV-122

The underside now uses 1.5 m longitudinal modules, six transverse strips and
restrained physical-scale grain in the existing concrete batch. Six-metre steel
frames have I-section beams, head plates, grounded columns, base plates and bolts.
Supports avoid entrances and the central name board; light centres sit between
structural bays. An intermediate terminal image caught a new column obscuring
the station name; it was repositioned and a regression assertion added.

The close live-board inspection produced ENV-122: the existing housing penetrated
the roof and its header was clipped. The complete board now sits below the beam,
with shortened grounded posts and bay-centred placement. All three text lines fit
inside the frame. Its support footprints also participate in passenger clearance.
Detailed display housing remains ENV-061, not closed by this layout repair.

Inspected matched rural, major-city, terminal, waterfront and underside views:
[soffit before](../build/visual-audit/canopy-structure-soffit-close-before.png) /
[after](../build/visual-audit/canopy-structure-soffit-close-after.png),
[clipped board](../build/visual-audit/canopy-board-clearance-before.png) /
[clear board](../build/visual-audit/canopy-structure-waterfront-after.png).
Final selected tests: **12/12, 13.08 s**, covering geometry, normals, ground/roof
connections, entrance/sign/light separation and complete display height bounds.
600-frame station: **118 draws unchanged**, **1,298,572 → 1,299,676** triangles;
steady mean **25.9 → 26.0 ms**, p95 **26.8 → 26.9 ms**, p99 **27.2 → 27.1 ms**,
maximum **27.4 ms unchanged**. Prefix: `canopy-structure-`. An interrupted capture
script was rerun completely; the final receipt belongs to the successful run.
ENV-052 and ENV-122 are addressed.

### Sealed canopy luminaires: ENV-053

Surface-mounted fixtures now have chamfered metal housings, a dark gasket, four
fasteners and a separate shallow diffuser. Only the diffuser is unlit; the casing
still responds to daylight. Emitters reuse the streamed world-luminaire pass that
already serves tunnels. No glow post-processing or new texture is introduced.

Inspected matched rural and waterfront images:
[before](../build/visual-audit/canopy-lights-waterfront-before.png) /
[after](../build/visual-audit/canopy-lights-waterfront-after.png).
Geometry tests check bounds, unit normals, downward emission, roof mounting and
the exact 124 housing / 32 diffuser triangle budget per fitting. Whole suite:
**63/63, 88.45 s**, including station exchange, reversal, six-car interactions and
material capture integrity. This is automated validation, not physical-input play.
600-frame station: **118 → 119 draws**, **1,299,676 → 1,300,108** triangles;
steady mean **26.1 → 26.0 ms**, p95 **27.0 → 26.9 ms**, p99 **27.4 → 27.2 ms**,
maximum **27.5 → 27.3 ms**. These compositor-paced runs do not prove 60 FPS.
Prefix: `canopy-lights-`. ENV-053 is addressed.

### Passenger-scale platform paving: ENV-054

Medium/High platforms now use staggered 600 mm concrete modules with 6 mm joints,
8 mm bevel widths and restrained slab-to-slab tint variation. Both banks use the
same inward safety-band clearance. Bevels meet the existing surface/joint bed;
all pieces share the concrete material draw. Low retains its cheaper expansion
grid and unchanged regression counts. `platform_joints` on Medium/High now counts
the laid modules rather than the old long seam bars.

Inspected matched close, oblique and doorway views:
[before](../build/visual-audit/platform-paving-paving-close-before.png) /
[after](../build/visual-audit/platform-paving-paving-close-after.png).
Selected tests: **12/12, 13.13 s**; new geometry assertions cover both banks,
bounded module count, upwards bevel normals, platform extents and invalid inputs.
600-frame station: **119 draws unchanged**, **1,300,108 → 1,320,184** triangles
(+1.54%); steady mean **26.1 ms unchanged**, p95 **27.0 → 26.9 ms**, p99
**27.5 → 27.2 ms**, maximum **27.7 → 27.4 ms**. Prefix: `platform-paving-`.
ENV-054 is addressed; tactile relief is the separate next item ENV-055.

### Raised tactile safety paving: ENV-055

Medium/High use 400 mm tactile modules with a six-by-six pattern of 27 mm wide,
5 mm high low-poly blisters. Actual relief is bounded to ±78 m around the station
centre, covering the passenger consist's close views; farther tiles retain grain
and joints. The edge has 260 mm pale coping, separate from the yellow warning
line. Low keeps a simplified surface. The existing opaque and concrete batches
are reused, without another draw or generated texture.

Inspected the matched [doorway before](../build/visual-audit/platform-tactile-doors-before.png) /
[after](../build/visual-audit/platform-tactile-doors-after.png). The band now reads
as a textured warning surface instead of a flat yellow rectangle. Selected tests:
**12/12, 13.28 s**, including millimetre height bounds, upward normals and a hard
population bound. 600-frame station: **119 draws unchanged**, **1,320,184 →
1,404,006** triangles (+6.35%); steady mean **26.1 → 26.0 ms**, p95 **26.9 ms**
and p99 **27.2 ms unchanged**, maximum **27.5 → 27.4 ms**. The geometry increase
is explicit; this is not a measured performance improvement. Prefix: `platform-tactile-`.
ENV-055 is addressed.

### Finished litter bins: ENV-056

The same small platform footprint now contains a recessed liner and opening lip,
folded cheeks, a sloped drip-edge lid, removable lower door, hinges, lock and kick
plate. The `LITTER` label reuses the station text atlas and draw. The bin footprint
also joins the shared passenger-clearance catalogue. No new asset or material.

Inspected [close before](../build/visual-audit/platform-bins-bin-close-before.png) /
[after](../build/visual-audit/platform-bins-bin-close-after.png), plus a corrected
supplemental left-bank oblique. The first oblique camera missed the bin and is
not evidence for that angle. Selected tests: **12/12, 13.20 s**, including bounded
shell geometry, liner depth, aperture clearance, ground contact and finite normals.
600-frame station: **119 draws unchanged**, **1,404,006 → 1,404,256** triangles;
steady mean **26.1 ms unchanged**, p95 **26.9 → 27.0 ms**, p99 **27.1 → 27.3 ms**,
maximum **27.3 → 27.6 ms**. Prefix: `platform-bins-`. ENV-056 is addressed.

### Supported rear fence runs: ENV-057

Rear boundaries are two explicit runs separated by the station access opening.
Each is divided into bays no longer than 6 m; posts exist at every bay boundary,
including both run ends. Rails now connect adjacent posts instead of projecting
from a centre post with disconnected free ends. Posts have small caps and base
plates that meet the new paving surface.

Inspected [before](../build/visual-audit/platform-fencing-fence-close-before.png) /
[after](../build/visual-audit/platform-fencing-fence-close-after.png), plus the far
platform view. Tests cover exact run endpoints, access-gap bounds, bay length,
grounding and cap geometry. Selected tests: **12/12, 13.29 s**. 600-frame station:
**119 draws unchanged**, **1,404,256 → 1,405,000** triangles; steady mean **26.0 →
26.1 ms**, p95 **26.9 → 27.1 ms**, p99 **27.2 → 27.4 ms**, maximum **27.6 ms
unchanged**. Prefix: `platform-fencing-`. ENV-057 is addressed.

### Shallow station-window interiors: ENV-058

Framed windows now have subdued room/sky gradients, shallow partitions, waiting
bench silhouettes and occasional part-lowered office shades. All cues stay behind
the existing frames and mullions on both banks. Entrance-door panes also have
graded glass and a quiet upper reflection. This is an opaque depth-cue approximation,
not a claim of transparent accessible station interiors or real-time reflections.
The terminal's separate large curtain-wall composition remains part of ENV-120.

Inspected matched city, rural, close and oblique windows:
[before](../build/visual-audit/station-glazing-rural-before.png) /
[after](../build/visual-audit/station-glazing-rural-after.png).
Final selected tests: **12/12, 13.35 s**, including front/back normals, inset depth,
variant budgets and existing entrance hardware bounds. 600-frame station:
**119 draws unchanged**, **1,405,000 → 1,405,088** triangles; steady mean **26.1 ms**
and p95 **26.9 ms unchanged**, p99 **27.1 → 27.3 ms**, maximum **27.5 → 27.6 ms**.
Prefix: `station-glazing-`. ENV-058 is addressed.

### Station material hierarchy: ENV-059

Upper urban walls use the existing physical-scale masonry material; other families
retain render behind their boarding or facade kits. Coherent family palettes now
distinguish wall, darker plinth and stone/painted trim. Closed plinth caps and roof
cornices wrap the mass; brick and terminal corners have restrained alternating
quoins. The lower front cap is split around the public entrance. No new textures
or draw passes are introduced.

Inspected matched Stonebridge, rural and major-city frames, including
[Northbridge before](../build/visual-audit/station-materials-major-front-before.png) /
[after](../build/visual-audit/station-materials-major-front-after.png).
Whole suite: **63/63, 89.23 s**; geometry tests cover both facade orientations,
door clearance, bounded corner detail and unit normals. 600-frame Stonebridge:
**119 draws unchanged**, **1,405,088 → 1,405,196** triangles; steady mean **26.1 ms
unchanged**, p95 **27.0 → 27.1 ms**, p99 **27.3 → 27.4 ms**, maximum **27.7 →
27.5 ms**. This station is a small rendered building; the images, not this one
triangle sample, establish the different major-city brick treatment.
Prefix: `station-materials-`. ENV-059 is addressed.

### Recognizable market-town clock turret: ENV-060

The old clock face was placed at the tower centre, hidden inside its solid mass.
The new face sits beyond the wall, with hour markers and hands driven by the same
service clock as platform clocks. Brick, corner masonry, coping, framed louvres
and a service entrance now give the lower visible mass architectural purpose.
The first doorway image exposed brick behind the door's trim kit; an opaque leaf
was added before acceptance. Final frames also include the subsequent ENV-061 case.

Inspected [tower before](../build/visual-audit/station-turret-turret-before.png) /
[after](../build/visual-audit/station-turret-turret-after.png) and matched doorway
views. Initial selected tests **13/13, 13.81 s**; final combined tests **13/13,
13.98 s**. Face-depth and normal tests prevent another hidden clock. Final
600-frame station, including ENV-061: **119 → 121 draws** (two clock hands),
**1,405,196 → 1,409,236** triangles; steady mean **26.0 ms unchanged**, p95
**26.9 → 26.8 ms**, p99 **27.1 → 27.2 ms**, maximum **27.3 ms unchanged**.
Prefix: `station-turret-`. ENV-060 is addressed.

### Finished departure-board housing: ENV-061

An eight-corner closed case replaces the flat board and two bars. It has an inset
screen, restrained metal bezel, flush fasteners, rear support brackets and bolted
foot plates. Text remains ahead of the screen and inside the readable rectangle;
the whole assembly retains ENV-122's clearance below the canopy. The dynamic
service-state update still rebuilds the same two small geometry/text batches.

Inspected matched [front](../build/visual-audit/platform-display-display-close-after.png)
and [oblique](../build/visual-audit/platform-display-display-oblique-after.png)
views against their preserved `before` counterparts. A first unit test incorrectly
excluded the projecting foot plate; its envelope was corrected, with the tighter
face/glyph clearance still checked above support height. Final selected tests:
**13/13, 13.98 s**. 600-frame station: **121 draws unchanged**, **1,409,020 →
1,409,236** triangles (includes the 12-triangle tower-door backing); steady mean
**26.1 → 26.0 ms**, p95 **27.1 → 26.9 ms**, p99 **27.3 → 27.4 ms**, maximum
**27.5 → 27.6 ms**. Prefix: `platform-display-`. ENV-061 is addressed.

### Graded terminal forecourts: ENV-062

Both route endpoints now have a paved public apron connecting the platform edge
to the city-side sidewalk, with six planted beds, four benches and a narrow
station-hall approach. Terrain-relative grading avoids floating edges. Feature
reservation excludes procedural buildings and vegetation from the apron, including
vegetation relocation candidates. Low quality retains the simpler station site.
An initial capture exposed triangular depth artifacts from overlapping grout and
tiles; non-overlapping joint strips and clipped bed boundaries replaced them.
Trees also received the existing branch and leaf-spray kit before acceptance.

Inspected final matched [Alderwick before](../build/visual-audit/terminal-forecourt-forecourt-outward-before.png) /
[after](../build/visual-audit/terminal-forecourt-forecourt-outward-after.png),
[Westhaven after](../build/visual-audit/terminal-forecourt-reverse-outward-after.png)
and seated terminal views. Final selected tests: **12/12, 13.44 s**; both endpoint
orientations, tile bounds/normals and deterministic footprint exclusion are covered.
600-frame Alderwick: **123 draws unchanged**, **1,422,288 → 1,456,678** triangles;
steady mean **25.9 → 26.1 ms**, p95 **26.8 → 26.9 ms**, p99 **27.1 → 27.4 ms**,
maximum **27.7 → 27.5 ms**. Initial world rebuild **115.8 → 155.2 ms** is a real
remaining synchronous-generation cost, not hidden in the steady metric.
The comparable baseline is `terminal-forecourt-terminal-performance-before.log`;
the earlier `terminal-forecourt-performance-before.log` measured Stonebridge and
must not be used as the terminal baseline. Prefix: `terminal-forecourt-`.
ENV-062 is addressed; the broader streaming hitch issue remains open.

### View-dependent water without detached streaks: ENV-063 / ENV-066

Medium/High water now uses a renderer-local OpenGL 3.3 effect: filtered analytic
ripple normals, bounded service-time phases, restrained sun response and a
view-dependent clear-sky Fresnel approximation. It is opaque single-pass water,
not reflected scene geometry, refraction or an additional render target. Low and
invalid-effect fallback retain the stock material. The initial image exposed an
overly regular wave pattern; phase modulation and reduced old-texture contrast
were applied before acceptance. Detached bright lake/river bars were removed at
all qualities instead of remaining superimposed on the new response.

Inspected matched [lake before](../build/visual-audit/water-surface-lake-oblique-before.png) /
[after](../build/visual-audit/water-surface-lake-oblique-after.png), river oblique,
passenger-window and waterfront views. Full suite: **66/66, 91.32 s**. Three new
quality tests require `water_shader_draws=0/1/1`; CPU tests cover finite, continuous
phase motion even at 1e12 service seconds and non-finite input fallback.
600-frame lake: **107 draws unchanged**, **1,108,650 → 1,108,038** triangles;
steady mean **26.1 → 26.0 ms**, maximum **27.5 → 27.6 ms**. Compositor-paced
measurements are not evidence of 60 FPS or a speedup. Prefix: `water-surface-`.
ENV-063 and ENV-066 are addressed; bank profiles ENV-064/065 remain separate.

### Grounded lake shore and bounded horizon continuation: ENV-064 / ENV-123

Additional shore samples preserve the graded basin instead of interpolating one
huge far-bank ramp. Bounded route-seeded bank irregularity and wet-earth/gravel/
vegetation contours replace the hard green-to-blue boundary. Contours split the
actual terrain triangles, preserving their planes, UVs, area and clockwise winding;
they are not coplanar overlays. The near water quad extends under dry ground so
the terrain defines the waterline. Rooted bent reed clumps replace isolated bars.
Extra lake samples match coarse neighbouring chunk edges.

The initial oblique view exposed ENV-123: an abrupt distant shoreline end. A
background-only terrain/water continuation now extends 500 m beyond each outer
detail chunk, beyond the nominal 1,050 m fog distance, without more simulation
chunks or draw submissions. The original defect is preserved in
[the discovery frame](../build/visual-audit/lake-bank-stream-edge-discovery.png).

Inspected matched [near bank before](../build/visual-audit/lake-bank-near-bank-before.png) /
[after](../build/visual-audit/lake-bank-near-bank-after.png), passenger and
[final oblique](../build/visual-audit/lake-bank-lake-oblique-after.png) views.
The `far-bank-before` camera lay below the old coarse terrain; that invalid angle
is explicitly excluded from acceptance. Whole suite **66/66, 94.31 s**; after
extracting the unchanged apron-range calculation and adding its invariant test,
selected tests **7/7, 7.45 s**. 600-frame lake: **107 draws unchanged**,
**1,108,038 → 1,119,348** triangles; steady mean **26.0 → 25.5 ms**, p95
**26.9 → 26.5 ms**, p99 **27.3 → 26.8 ms**, maximum **27.4 → 27.6 ms**.
The initial rebuild increased **66.2 → 88.2 ms**; steady timing variation is not
claimed as a speedup. Seven chunks remain loaded. Prefix: `lake-bank-`.
ENV-064 and ENV-123 are addressed; ENV-065 remains open.

### Natural river-bank transition: ENV-065

Independent bounded phases vary the two estuary banks instead of making a
symmetrical straight channel. The shared terrain-contour method provides a wet
earth/gravel transition before field vegetation. Medium/High place reed clumps
and intermittent shrubs on the actual rendered bank surface, using stable
absolute route slots. No new texture or draw pass is needed; Low retains a
simpler bank without the added clumps. `river_bank_clumps` exposes the population.

Inspected matched [right bank before](../build/visual-audit/river-bank-river-oblique-before.png) /
[after](../build/visual-audit/river-bank-river-oblique-after.png), left-bank oblique
and passenger-window views. Final full suite: **67/67, 102.90 s**, including a new
river-bank population smoke and bounded bank-profile tests. 600-frame river run:
**95 draws unchanged**, **940,240 → 948,040** triangles, **98 bank clumps**;
steady mean **25.4 → 25.7 ms**, p95 **25.8 → 26.5 ms**, p99 **25.9 → 27.1 ms**,
maximum **26.0 → 27.5 ms**; initial rebuild **49.9 → 51.9 ms**. These are bounded,
compositor-paced stationary measurements, not moving-world or 60 FPS evidence.
Prefix: `river-bank-`. ENV-065 is addressed. Work stopped at the owner's explicit
request after this item and resumed on 2026-10-10 with ENV-067.

### Finished bridge parapets: ENV-067

Both crossings replace the stacked flat edge beams with one shared parapet in
`BridgeGeometry.hpp`: a concrete upstand, an overhanging coping with a recessed
drip line, an outer cornice, an inboard cable-trough lid and a galvanised
two-rail tubular handrail on 2.5 m posts with base plates. Rail ends are capped
only where a section actually begins or ends a crossing. The river bridge keeps
its darker steel fascia below the upstand. Low quality omits base plates and
trough joints; `bridge_parapet_posts` exposes the population. An initial 2 m post
cadence cost 1,550 posts and a 47 ms larger initial rebuild on the viaduct and
was rejected before acceptance.

Inspected matched [river left-window before](../build/visual-audit/bridge-parapet-river-left-before.png) /
[after](../build/visual-audit/bridge-parapet-river-left-after.png), the seated
downward view, river oblique, [viaduct downward before](../build/visual-audit/bridge-parapet-viaduct-down-before.png) /
[after](../build/visual-audit/bridge-parapet-viaduct-down-after.png) and the viaduct
oblique. The lower window now shows a thin rail silhouette above a finished
coping instead of two grey bars. Three new unit tests cover the valley profile,
tube normals/caps and the parapet envelope at both deck edges. 600-frame viaduct
sample: **95 draws unchanged**, **1,223,684 → 1,259,312** triangles, **1,240
posts** (viaduct window run); steady mean **25.4 → 25.4 ms**, p95 **26.0 →
25.7 ms**, p99 **26.2 → 26.0 ms**, maximum **26.3 → 26.9 ms**; initial rebuild
**127.4 → 145.6 ms**. Prefix: `bridge-parapet-`. ENV-067 is addressed.

### Bowl-shaped viaduct valley and footed piers: ENV-068

The dry crossing previously lowered the terrain to one fixed depth along the
whole 2.8 km span, so from the window the viaduct ran over a sunken plain and
an exterior view showed a floating field edge beside the abutment. The valley
profile now lives in `BridgeGeometry.hpp` and is used by the single terrain
height function: natural approach terrain blends, with zero weight at both
abutments, into a bowl deepest near mid-span, closed in by side spurs whose
phases differ per valley side. Viaduct pier shafts thicken with height, stand
on footings, and tall piers gain tie beams, so they read as structure rather
than two thin posts. The streamed chunk count, scenery clearance and feature
reservation are unchanged.

Inspected matched [valley floor before](../build/visual-audit/viaduct-valley-valley-floor-before.png) /
[after](../build/visual-audit/viaduct-valley-valley-floor-after.png),
[along-track before](../build/visual-audit/viaduct-valley-valley-along-before.png) /
[after](../build/visual-audit/viaduct-valley-valley-along-after.png),
[underside before](../build/visual-audit/viaduct-valley-valley-underside-before.png) /
[after](../build/visual-audit/viaduct-valley-valley-underside-after.png), both
abutments and the diagonal view. The old underside view exposed sky under a
floating ground plate; the new one is grounded. From the passenger window the
far ground now rises toward the horizon on both sides. Note that the valley
captures were taken with the same before binary as the parapet set, so their
before images already lack the parapet change only in the sense that neither
repair was present. 600-frame viaduct sample (both repairs): steady mean
**25.3 ms**, p95 **25.6 ms**, p99 **26.0 ms**, maximum **26.2 ms**, initial
rebuild **139.5 ms**; triangle and draw counts match the parapet run. These are
compositor-paced stationary measurements, not moving-world evidence. Whole
suite after the final build: **67/67, 105.79 s**. Prefix:
`viaduct-valley-`. ENV-068 is addressed.

### Precast tunnel lining, wall fixtures and utility markers: ENV-069 / ENV-070 / ENV-071

The tunnel walls were plain lit boxes with a 20 m joint, a bright rectangle
in a dark patch every 10 m and two unlabelled bars. `TunnelGeometry.hpp` now
adds, per 20 m structural segment and wall, a textured precast face in the
streamed concrete material (1.6 m tiling), a roof haunch, two longitudinal
joints and absolute 2 m ring joints with Medium/High bolt pockets;
`tunnel_lining_rings` counts them. Each luminaire is a bracketed housing with
end caps and a thin emitter lens; its wall wash is four gradient quads in the
same textured batch whose outer colour equals the lining tint, so the falloff
has no visible seam. The handrail and cable tray are lit steel on 4 m brackets
with two cable runs. Every 100 m niche bay now carries a green escape-route
sign giving the direction and distance to the nearer portal (computed from the
authored extents exported by `Route.hpp`) and a hectometre plate, both in the
antialiased printed-text batch; `tunnel_marker_plates` counts them. The first
build printed the labels as pixel boxes and cost 108k triangles; flat printed
glyphs were substituted before acceptance.

Inspected matched [window before](../build/visual-audit/tunnel-lining-window-before.png) /
[after](../build/visual-audit/tunnel-lining-window-after.png), left window,
[fixture close before](../build/visual-audit/tunnel-lining-fixture-close-before.png) /
[after](../build/visual-audit/tunnel-lining-fixture-close-after.png), the
along-wall view and, under the `tunnel-markers-` prefix captured with the
same before binary, the [123.0 km bay before](../build/visual-audit/tunnel-markers-niche-bay-before.png) /
[after](../build/visual-audit/tunnel-markers-niche-bay-after.png), its left-wall
twin and an oblique. The escape sign reads "< 2.0 KM" at that bay. Three new
unit tests cover label/extent logic, lining envelope and orientation, and the
lens/wash construction; the tunnel environment smoke now requires both
counters. 600-frame tunnel sample: **87 → 89 draws** (concrete and printed
batches now present in the tunnel), **783,628 → 839,908** triangles, **1,750
rings**, **68 plates**; steady mean **25.4 → 25.4 ms**, p95 **25.9 → 25.6 ms**,
p99 **26.8 → 27.2 ms**, maximum **32.5 → 32.2 ms**; initial rebuild **17.6 →
22.9 ms**. Prefixes: `tunnel-lining-`, `tunnel-markers-`. ENV-069, ENV-070 and
ENV-071 are addressed.

### Loose ballast, sloped shoulders, monoblock sleepers and rail section: ENV-072 to ENV-075

`makeBallastDetailImage` now jitters stone centres across whole cells, stretches
a third of them, lights each stone across its own facet from a per-stone
direction, sinks a ninth of them into shadow and separates neighbours by a thin
crevice instead of a broad dark boundary; the tile grew from 0.72 m to 0.88 m.
The texture remains neutral, deterministic and mip-friendly. `TrackGeometry.hpp`
adds the cross-sections: the formation box is narrowed under the crown and both
shoulders are sloped textured quads from the 2.66 m crown edge down to a toe
whose lateral wobbles deterministically between 3.1 and 3.6 m, so consecutive 6 m
segments meet without a ruled edge. Sleepers inside the existing fastener radius
are chamfered monoblocks (two rail-seat blocks on a lower, narrower waist, 90
indices instead of 36); farther sleepers remain single boxes. Each rail is a
broad oxidised foot, a 18 mm web and a 72 mm head rising to 148 mm, with the
specular running-surface quad on top of the head.

Inspected matched [track close before](../build/visual-audit/track-surface-track-close-before.png) /
[after](../build/visual-audit/track-surface-track-close-after.png),
[shoulder before](../build/visual-audit/track-surface-shoulder-before.png) /
[after](../build/visual-audit/track-surface-shoulder-after.png), the along-track
view with both running lines and the signal view. The stones read as loose
angular aggregate, the dark curb is replaced by a graded shoulder, sleepers show
lit tops and shaded chamfers, and the rail reads as a section with a bright head
over a dark web. Three new unit tests cover the toe bounds/continuity, the
sleeper envelope, chamfer and outward normals, and the rail profile widths; the
ballast material test still passes. 600-frame rural sample: **94 draws
unchanged**, **1,313,118 → 1,392,266** triangles (sleeper profiles and
shoulders; ballast material **1,336 → 4,008**); steady mean **25.4 → 25.5 ms**,
p95 **25.7 → 26.3 ms**, p99 **25.9 → 26.7 ms**, maximum **26.3 → 27.2 ms**;
initial rebuild **189.4 → 134.6 ms** (run-to-run variation, not a claimed
speedup). Prefix: `track-surface-`. ENV-072 to ENV-075 are addressed.

### Wire fences, signal construction and joined catenary: ENV-076 to ENV-080

`LinesideGeometry.hpp` adds `addBarBetween`, a square bar between two world
points with yaw and pitch, and the boundary fence: 75 mm concrete posts on an
absolute 3 m cadence, four 18 mm strands as 12 m chords, and a heavier strained
post with a diagonal strut at either end of a run whose neighbouring chunk is a
bridge, tunnel or urban corridor; `fence_posts` counts them. The contact wire
and sagged messenger of both running lines are now chords between the same
absolute 12.5 m (High) or 25 m points, with the sag evaluated at each end, so
the earlier 252 m straight boxes no longer step sideways at chunk seams on
curves. `SignalGeometry.hpp` rebuilds the colour-light signal: bolted base
flange, mast access panel, white-edged backboard over a slimmer housing, lens
rims and hoods, a slim ladder on stand-off brackets with a landing and hoop
guard, a cabinet with hinges, lock, louvres and conduit, and the identification
plate. The three recessed lens centres and the `S` number text keep their exact
positions, so the live aspect overlay is unchanged. Low quality omits the bolts,
hinges and louvres.

Inspected matched [signal before](../build/visual-audit/lineside-signal-before.png) /
[after](../build/visual-audit/lineside-signal-after.png),
[fence close before](../build/visual-audit/lineside-fence-close-before.png) /
[after](../build/visual-audit/lineside-fence-close-after.png), the fence along
view and the three wire views; the oblique train-side before image shows the
stepped wire runs, the after image continuous wires. Three new unit tests cover
the chord bar envelope, fence cadence/strainers/`linesideFenceAllowed`, and the
signal lens positions at both facings and sides. 600-frame rural sample: **94
draws unchanged**, **1,392,266 → 1,422,530** triangles, **1,166 fence posts**;
steady mean **25.5 → 25.5 ms**, p95 **26.2 → 25.8 ms**, p99 **26.6 → 25.9 ms**,
maximum **29.4 → 26.2 ms**; initial rebuild **126.6 → 139.8 ms**. Prefix:
`lineside-`. ENV-076 to ENV-080 are addressed.

### Close passenger fittings: ENV-081 to ENV-092

`InteriorFittingsGeometry.hpp` collects the close-inspection fittings; the seat
rear shell changes live in `InteriorGeometry.hpp`. Grip recess: a dark cavity
inside a proud bevelled rim with a shadow strip along the upper inner edge and a
lit strip along the lower one. Literature pocket: flush fabric back, an elastic
front quad leaning from the shell to a stitched mouth band 34 mm out, two side
gussets and a leaflet with a coloured header; the shell stays inside its 55 mm
rear envelope and 600-triangle budget, so strips are plain boxes. Seat support:
two-step bevelled base disc, boss, collared twelve-sided column and upper collar
within the earlier footprint. Luggage: rounded shells, seam band, corner guards,
loop handle, zip line and castors on alternate cases. Cab door: recessed leaf in
a proud frame with gap lines, hinge knuckles, kick plate, escutcheon, round-stem
lever and lock cylinder below the warning band (the first build placed the lever
over the STAFF ONLY frame and was corrected). Extinguisher: strap bracket with
base cup, hose clipped down the body with nozzle, gauge on the valve. Intercom:
bezelled housing, domed red button in a raised ring, bezelled answer lamp and
printed EMERGENCY / PRESS AND SPEAK legends. End-wall fasteners: 14 mm heads
close to the panel tone in darker countersinks. Gangway: a grooved crowned
bridge plate with hinge lines and yellow nosings; domed caps on both grab rails.

Inspected matched seat rear [before](../build/visual-audit/interior-fittings-seat-rear-detail-before.png) /
[after](../build/visual-audit/interior-fittings-seat-rear-detail-after.png),
pedestal before/after, car aisles 1/5/6, vestibule and gangway, plus the
`interior-close-` views captured with the same before binary: luggage, the
[cab door before](../build/visual-audit/interior-close-cab-door-close-before.png) /
[after](../build/visual-audit/interior-close-cab-door-close-after.png), the car 6
end wall with extinguisher and intercom, and the intercom close view. Four new
unit tests cover the pedestal footprint, suitcase envelope, cab-door hardware
projection/side and the plate/intercom bounds; the existing seat-shell envelope
and budget tests still pass. 600-frame seated car 3 sample: **94 draws
unchanged**, **1,422,530 → 1,432,962** triangles; steady mean **25.5 → 26.6 ms**,
p95 **26.1 → 31.7 ms**, p99 **26.4 → 35.6 ms**, maximum **27.4 → 47.4 ms**. The
after run coincided with a host load average above 24 from other agents; the
same binary measured p95 **25.8 ms** / p99 **27.0 ms** in an earlier quieter
run (`interior-close-performance-after.log` of that pass was overwritten by the
final capture), and the geometry delta is 0.7 %, so the slower figures are
attributed to host load rather than the fittings. Re-measure on a quiet host
before treating this as a regression. Prefixes: `interior-fittings-`,
`interior-close-`. ENV-081 to ENV-092 are addressed.

### Accessible toilet compartment: ENV-093 to ENV-104

`ToiletGeometry.hpp` replaces the household-style fixtures. The toilet is a
shrouded pedestal with a dark kick recess, an oval bowl (the shared
`addSanitaryBowl`), a flat seat ring with hinge block, a raised lid and a
stainless cistern cover carrying a bezelled flush button and label plate. The
support is a rounded tube with a return and brackets; a paper dispenser with a
visible roll end and tongue sits beside the bowl; 45-degree coves close all four
wall/floor junctions. The basin now sits in a counter with a rear upstand over
an enclosed vanity with kick recess and waste flap, under a chrome post and
horizontal sensor spout with sensor window and aerator, with a strainer ring and
dark outlet in the drain. The mirror keeps its frame, gains bevels and shows a
ceiling-to-teal gradient with a slanted highlight; soap and towel dispensers have
a sight window, push plate, nozzle, slot and hanging towel. A brushed threshold
plate with ramps and a seal joins the two floors at the doorway. Alarm call
point, bin, pull cord and grille are unchanged.

Inspected matched [toilet before](../build/visual-audit/toilet-fittings-toilet-before.png) /
[after](../build/visual-audit/toilet-fittings-toilet-after.png),
[sink before](../build/visual-audit/toilet-fittings-sink-before.png) /
[after](../build/visual-audit/toilet-fittings-sink-after.png), bowl close and
side, sink close, flush/dispenser and threshold views. The `mirror` pose in the
capture script lands inside the vanity and shows a flat surface in both stages;
it is not acceptance evidence and the sink view covers the mirror. Two new unit
tests bound every fitting inside the compartment envelope and check that the
mirror gradient is paler toward the ceiling. 600-frame seated car 4 sample:
**94 draws unchanged**, **1,432,962 → 1,433,892** triangles; steady mean
**25.4 → 25.4 ms**, p95 **25.9 → 26.1 ms**, p99 **26.2 → 26.3 ms**, maximum
**36.7 → 30.5 ms**. Prefix: `toilet-fittings-`. ENV-093 to ENV-104 are
addressed.

### Ceiling HVAC diffusers: ENV-121

`addCeilingDiffuser` in `InteriorFittingsGeometry.hpp` replaces the thick
block with dark slots: a 22 mm trim frame stands 8 mm below the ceiling with a
fastener at each corner and a centre divider, the plenum is a dark recess
behind the ceiling plane, and fourteen angled light-toned slats lean from the
plenum toward the frame plane with visible gaps between them. The first build
wound the slats upward and they were invisible from the saloon; the unit test
now requires every slat normal to point down. The footprint, the `hvac_vents`
count and the shared interior batch are unchanged.

Inspected matched [ceiling before](../build/visual-audit/hvac-grille-ceiling-before.png) /
[after](../build/visual-audit/hvac-grille-ceiling-after.png) and the `hvac-close-`
pair of close and oblique diffuser views captured with the same before binary.
Unit tests: **172/172**; whole suite **67/67, 122.69 s**. 600-frame seated car 2
sample: **92 draws unchanged**, **1,433,508 → 1,434,132** triangles; steady mean
**25.4 → 25.5 ms**, p95 **25.8 → 25.8 ms**, p99 **26.1 → 26.0 ms**, maximum
**26.3 → 26.4 ms**. Prefixes: `hvac-grille-`, `hvac-close-`. ENV-121 is
addressed; ENV-105 to ENV-120 remain open.
