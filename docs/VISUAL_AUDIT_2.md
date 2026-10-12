# Second screenshot-based audit (pass 2)

Baseline: 2026-10-12, commit `7afeb66`, CNA `next` / OpenGL 3.3, High quality, 1280 × 720,
private hardware GPU (Radeon 780M, Mesa 25.0.7) through `run_gpu_tests_private.sh`.
This pass starts from new engine screenshots after the first campaign (ENV-001 to ENV-123,
all closed in [VISUAL_AUDIT.md](VISUAL_AUDIT.md)). Closed IDs are not reopened; new findings
continue the numbering from **ENV-124**. The owner asked for a new audit together with work on
the remaining design-level gaps (repetitive procedural buildings, flat materials, stylised
avatars) and an analysis of the remaining streaming risks ([STREAMING.md](STREAMING.md), PERF-002).

## Evidence and limits

[Capture script](../tools/capture_visual_audit_pass2.sh) records 63 poses with the prefix
`audit2-` in `build/visual-audit/`, plus a binary SHA-256 receipt (`audit2-binary.sha256`):
every car aisle, ceiling, cab bulkhead, vestibule, gangway, toilet and sink; three populated
frames with CNA avatars; five train-exterior poses; all twelve station platforms and a forecourt
aerial; sixteen seated window views between km 2 and km 270 on both sides; lake, river bridge,
viaduct and tunnel approach; town and countryside aerial/ground compositions. The four window
views at non-round distances (`audit2-window-km{30.13,101.37,163.81,212.55}-right`)
separate real repetition from the alignment of round-kilometre poses with the 250 m chunk grid;
they were taken in a follow-up run of the same binary and are now part of the script.
All images were inspected as real engine output (labelled 2 × 2 contact sheets in
`build/visual-audit/audit2-sheets/` and full-resolution frames for close judgements).

Static frames use effectively frozen route time and no audio. Populated frames run 240 frames at
0.01× time so avatars can load and sit. As in pass 1, these images do not establish animation,
flicker, audio or long-run behaviour.

P1 = persistent passenger view, P2 = prominent route or station view, P3 = occasional view,
P4 = exterior audit view the passenger normally cannot take.

## Ranked findings

Current disposition: **10 addressed, 0 open** (see the repair evidence below). As in pass 1, this
does not claim photographic realism; each listed visible problem was repaired and inspected.

| Rank / ID | Priority | Status | Visible problem and intended acceptance direction | Evidence |
| --- | --- | --- | --- | --- |
| 124 ENV-124 | P1 | ADDRESSED | The rural verge between ballast toe and fence, and the headland beyond the fence, are single-colour 50 m terrain cells that repeat unchanged along the whole route; they fill the lower third of every seated window view. Give the verge a cess strip, mown and rough grass zones and patchy tonal variation at metre scale, plus low wildflower and weed cover under the fence. | window-km45-right, window-km30.13-right, window-km101.37-right, window-km212.55-right |
| 125 ENV-125 | P1 | ADDRESSED | In urban sections the ground between track and street is a flat uniform grey plane. Separate a cess path, cable troughing and a sparse weedy verge from the road edge. | window-km2-right, window-km163.81-right |
| 126 ENV-126 | P1 | ADDRESSED | Interior signs (GANGWAY, STAFF ONLY, WC / ACCESSIBLE WC) still use coarse dotted pixel lettering although passenger displays and station names were moved to antialiased type. Use the same antialiased face for fixed signage. | gangway, vestibule, car-1-aisle, car-6-aisle, sink, populated-car-2 |
| 127 ENV-127 | P2 | ADDRESSED | Every rural farmhouse is the same two-storey box with one red gable roof, three-by-two windows and a centred door; it recurs at km 60, 100, 140 (both sides), 160 and 220. Introduce distinct farmstead types, roof forms and colours, chimneys and outbuildings while keeping the existing placement budget. | window-km60-right, window-km100-right, window-km140-left, window-km140-right, window-km160-right, window-km220-right, lake |
| 128 ENV-128 | P2 | ADDRESSED | Field-boundary hedges stand at exactly every 500 m, perpendicular to the line on both sides and aligned with the parcel tint grid, so the landscape reads as a regular grid at speed. Vary boundary spacing, angle and type (hedge, tree line, open margin) per side. | window-km15-right, window-km30-right, window-km45-right, window-km200-right, window-km250-right, country-aerial |
| 129 ENV-129 | P2 | ADDRESSED | Urban apartment blocks share one blue-grey facade tone and identical punched window grids, without balconies, entrance canopies or a differentiated ground floor. Add restrained palette variation and storey detail on near blocks. | window-km2-left, town-aerial, town-street, window-km163.81-right |
| 130 ENV-130 | P2 | ADDRESSED | Arable parcels are flat single-tone blocks; at distance cultivated land, stubble and bare soil differ only in hue. Add crop rows and tramlines aligned per parcel. | country-aerial, viaduct, window-km45-right |
| 131 ENV-131 | P2 | ADDRESSED | All twelve stations share one canopy: the same slate soffit, beams and column colour. Station identity comes only from the back wall. Give each station family its own canopy colour scheme and edge treatment. | station-1 to station-12 |
| 132 ENV-132 | P3 | ADDRESSED | Commercial and industrial sheds are dark grey uniform boxes, reading as unlit silhouettes even in daylight. Use lighter cladding tones with variation and visible base/roof trims. | station-7-forecourt, window-km163.81-right |
| 133 ENV-133 | P4 | ADDRESSED | Each driving cab side is a single 7 m glass pane over a plain teal box, so from outside the cab reads as an open glass box; the livery stops at the bulkhead. Close the cab sides with a cab door, a drop-light window, solid equipment-room panels and continued livery. | train-front |

Observed but not listed as repair findings:

- **Avatars** are CNA Gamer Services catalog avatars. Their stylised proportions and faces come
  from the immutable dependency catalog; the project already selects clothing and adjusts
  seated proportions (see `bugs.md`). In the inspected populated frames the seated poses sit
  on the cushions without visible clipping, so no project-side defect is listed.
- The interaction prompt (`E SIT DOWN`, `E STAND UP`) remains visible with `--hide-hud`. It is
  part of the capture harness view, not of the world, and is not a visual finding.

## Repair evidence

All repairs share one matched capture set from
[`tools/capture_audit2_repairs.sh`](../tools/capture_audit2_repairs.sh): `before` was captured with
the baseline binary (SHA-256 `cbb71bdfb575cabc70095faa79f32021e6b902089a2a1de287e0dda59b5c25cf`,
the `audit2` build of commit `7afeb66`) and `after` with the final binary
(`3e89842194b2ee994aabca6e64545cc164a51a891a30ceebc8fc36a122648f0e`). Keys below are
`build/visual-audit/audit2-repairs-<id>-{before,after}.png`. Five supplementary poses
(`cab-rear`, `canopy-rural`, `canopy-waterfront`, `tramlines`, `verge-flowers`) were added after
the `before` stage and exist only as `after` images. Every pair was inspected at full resolution
or in the labelled comparison sheets `build/visual-audit/audit2-sheets/final-*.png`.

Shared verification for every group: render/geometry **193/193**, simulation **128/128**, full
CTest **69/69** in 149.9 s (`build/visual-audit/audit2-ctest-final.log`). Performance on the
private Radeon 780M, High, window view:

| Sample | Before | After |
| --- | ---: | ---: |
| 600-frame stationary, km 52: triangles / draws | 1,457,752 / 94 | 1,560,960 / 95 |
| stationary steady mean / p95 / p99 / max (ms) | 25.4 / 25.8 / 26.1 / 26.6 | 25.6 / 26.2 / 26.6 / 26.9 |
| 1,800-frame ride at 8x from km 52: triangles at end | 1,551,512 | 1,597,314 |
| ride steady mean / p95 / p99 / max (ms) | 25.4 / 25.9 / 27.3 / 55.9 | 25.6 / 26.4 / 26.8 / 27.8 |
| prepared world swaps / late / largest upload | 45 / 0 / 41.5 ms | 45 / 0 / 17.0 ms |
| kernel peak RSS | 876,572 KiB | 852,428 KiB |

The ~26 ms cadence is the private compositor's synchronised presentation (see `bugs.md`), not a
60 FPS claim. The ride improvements come from the streaming work in
[PERF-002](STREAMING.md#perf-002--remaining-streaming-risks); the visual additions cost about
3–7% more triangles.

### Rural verge, field boundaries and tramlines: ENV-124 / ENV-128 / ENV-130

`VergeGeometry.hpp` gives the near terrain cells (ballast toe to 26 m) metre-scale zones in
absolute route coordinates: a grey-brown cess, a mown strip with lush, clover-dark and dry
patches, an unmown fence line with seed heads, a damp drain strip and a headland that ends on
exactly the field-margin colour, so the field surface beyond continues without a seam.
`addGridTerrainCell` (in `TerrainPatchGeometry.hpp`) subdivides those cells and splits any
sub-rectangle on the source diagonal, so terrain height and collision never change (unit test).
The running-line band uses a 3 m detail tile for grass-scale grain. A precast cable trough runs
on the open cess behind the 4.3 m catenary masts, and wildflower clumps grow in drifts along the
inner fence line; both stop at platforms, structures and level crossings.

Field parcels (`ruralParcelBoundary` / `ruralParcelIndex`) now follow a nominal 500 m rhythm
shifted per side by whole 50 m terrain cells, so boundaries stay on cell edges where the
subdivided field surface already has margin samples. `fieldBoundaryLayout` picks a hedge, a
hedge with a field gateway, a hedgerow with standard trees, or an open grass margin, each with
its own extent (96–180 m). Stubble and ploughed parcels (`ruralParcelArable`, sharing the
palette identity) carry paired tramlines every 24 m, kept off headlands, buildings and crop
plots.

Inspected: `verge-km45`, `verge-km101`, `verge-km212` and `verge-close` show patchy zones, the
trough and flower drifts instead of one flat green; the identical diagonal hedge leaving the
fence at every round kilometre is gone (`verge-km45`, `farm-km60`, `farm-km100`); `country-wide`
and the supplementary `tramlines` frame show worked fields; `country-aerial` shows an open
margin and a farm with outbuilding. Limits: the verge remains vertex colour on a neutral detail
texture, so it reads as patchy grass rather than individual blades; flower heads are small at
seated distance; tramlines are only on two of the six parcel types and are faint at long range.
Counters: `cable_trough_units`, `verge_flower_clumps`, `field_boundary_trees`, `tramline_segments`.

### Urban lineside: ENV-125

Inside urban sections the near cells use `urbanVergeTint`: gravel cess, weed-grown ballast
dust and a scrub strip that joins the street-side ground colour at 12 m. A steel palisade
(posts on an absolute 2.75 m cadence, two rails, pointed 60 mm pales every 250 mm, pales on High
only) separates railway and street on both sides at 11.55 m; spans are drawn by the chunk that
holds their first post, so runs continue across chunk seams and end on a post at stations,
structures and level crossings. Inspected: `urban-right`, `urban-left`, `urban-km163` and
`town-aerial`; the street and frontages remain visible through the palisade. Limit: the
palisade is one generic design for every town.

### Antialiased fixed signage: ENV-126

GANGWAY, STAFF ONLY, ACCESSIBLE / WC and the intercom legends now use the licensed passenger
display atlas through `addPrintedPlaqueText` (`PassengerDisplayText.hpp`) in a separate
`interiorLettering_` mesh drawn with the existing display effect. The dot-matrix helper was
removed. Inspected: `gangway`, `vestibule`, `cab-end`, `sink`. Kilometre posts and a few
track-facing exterior labels still use pixel glyphs; they are outside this finding.

### Farmsteads and houses: ENV-127

`houseStyle` derives facade (7) and roof (5) palettes, bungalow or two storeys, door bay,
chimney layout, porch and roof pitch from a route-anchored seed; `houseEnvelope(scale,style)`
keeps the reserved footprint, and the projected roof shadow uses the same envelope. Rural houses
whose variant has bit 2 set own a detached barn or rendered garage 13 m along the route
(`houseHasOutbuilding` in `StreamingWorld.hpp`); its footprint joins the parcel list, so trees
are moved off it like any other building. Inspected: `farm-km60`, `farm-km100`, `farm-km140`,
`farm-km160` and `country-aerial` now show different houses, roofs and outbuildings. Limit: all
houses still share one rectangular plan and window module.

### Urban blocks, sheds and shaded facades: ENV-129 / ENV-132

`blockStyle` gives apartment blocks eight render colours, three plinth tones, three railing
colours and four balcony rhythms; near blocks gain a plinth skin, a first-floor string course
and projecting balconies with railings. Towers draw from six facade colours and carry a spandrel
band on every floor of the railway face. Warehouse cladding moved to lighter profiled tones.

Most building faces seen from the train are turned away from the sun, where only the cool sky
ambient lit them; every shaded facade became the same blue-grey. `exteriorLighting` now adds a
warm ground-bounce light travelling against the sun's horizontal heading and slightly upward
(`groundBounceDirection`), and a weaker uplight from below (`groundUplightDirection`). Neither
reaches terrain, roofs or any up-facing surface (unit tested); sunlit faces are unchanged. Both
the fixed-function batches and the OpenGL 3.3 facade shader apply them; they are zero in tunnels.
Inspected: `urban-left`, `urban-km163`, `town-aerial`, `forecourt`. Limits: blocks keep a single
punched-window module; shaded faces are brighter and warmer but not physically lit.

### Station canopies: ENV-131

`canopyPalette` gives each station family its own deck, fascia, steelwork and soffit colours:
heritage green steel with cream boarding and a timber dagger-board valance on rural stops, white
steel with a blue fascia at the waterfront stations, graphite with a maroon fascia at major
urban stations; suburban stops and the termini keep the original scheme. The uplight brightens
the formerly slate-dark soffits. Inspected: `station-1`, `station-4`, `station-5`, `station-7`,
`station-9`, `station-11` and the supplementary `canopy-rural` / `canopy-waterfront` frames.
Limit: from the seated platform view the change is mostly the steel colour and soffit tone;
the valance is visible on approach and from the platform.

### Driving-cab bodyside: ENV-133

`addCabBodyside` (`TrainExteriorGeometry.hpp`) replaces the single 7 m pane over a plain teal
box: a solid equipment-room wall with the dark window-band skin and a louvred grille, a hinged
cab door with drop-light, perimeter seal, grab handles and kick step, a fixed side window, the
passenger cars' lower band and livery, and one bogie under each cab (`addLowerBodyside` can now
omit either bogie). Glazing per side fell from about 9.5 m² to under 3.5 m² (unit test).
Inspected: `cab-front`, `cab-side` and the supplementary `cab-rear`. This exterior is only seen
from audit cameras; the passenger view of the cab through the bulkhead window is unchanged.

### Observed limits not listed as findings

Avatar style remains the CNA catalog's; no seated clipping was seen in the populated frames.
The capture harness keeps the interaction prompt visible with `--hide-hud`.
