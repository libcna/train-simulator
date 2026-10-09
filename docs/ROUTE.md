# Northern Valley Express route

The Northern Valley Express is a fictional 286.4 km electrified intercity corridor. A complete
one-way run is compressed into an enjoyable game session by the default 2.4 simulation time scale
while preserving a 160 km/h top speed, 0.62 m/s² acceleration, and 0.78 m/s² service braking.

| Km | Station | Character | Run | Limit | Dwell | Platform eastbound |
|---:|---|---|---:|---:|---:|---|
| 0.0 | Alderwick Central | Major terminal | — | — | 90 s | Left |
| 18.6 | Merefield | Small town | 10.1 min | 120 km/h | 45 s | Right |
| 39.8 | Hawthorn Park | Suburban | 10 min | 140 km/h | 50 s | Left |
| 63.2 | Greenford Vale | Rural town | 9.8 min | 160 km/h | 55 s | Right |
| 88.7 | Lakehurst | Regional hub | 10.6 min | 160 km/h | 60 s | Left |
| 111.9 | Pine Crossing | Rural stop | 10.9 min | 140 km/h | 40 s | Left |
| 138.3 | Stonebridge | Market town | 10.9 min | 160 km/h | 55 s | Right |
| 165.8 | Meadow Junction | Major junction | 11.4 min | 160 km/h | 65 s | Left |
| 192.4 | Northbridge | City station | 12.3 min | 140 km/h | 50 s | Right |
| 218.9 | Redwater | Riverside town | 11 min | 160 km/h | 45 s | Left |
| 247.6 | Eastmere | Outer city | 13.25 min | 140 km/h | 55 s | Right |
| 286.4 | Westhaven Central | Major terminal | 15.6 min | 160 km/h | 120 s | Left |

Platform side is mirrored naturally when direction reverses. Development builds may use
`--development-dwell`, which scales dwell time to eight percent without altering the production
timetable.

The published departure-to-arrival schedule is about 135 simulated minutes including intermediate
dwells, or roughly 56 real minutes at the default time scale. The service state keeps an absolute
scheduled arrival for the next station, the last arrival deviation, and a live predicted deviation;
the expanded passenger-information panel reports whether the train is on time, early, or late.

Authored features include a river bridge at km 73.9-75.1, a level lakeside approach at km
80.5-87.2, a long tunnel at km 121.0-126.8, a valley viaduct at km 177.2-180.0, and a short tunnel
at km 229.5-231.1. The Lakehurst water body sits beside rather than beneath the railway: route data
owns its side and absolute elevation, terrain forms a deterministic basin, and streamed water,
reeds, glints and a wooded far bank make it a sustained window-view landmark. Station-centered
Gaussian urban envelopes drive deterministic building density, while rural chunks favor vegetation.
Sparse seeded farm compounds add fenced yards, pitched-roof barns, houses, silos and hay storage
well beyond the railway corridor without turning every agricultural chunk into a settlement.
Separate bounded straw-bale clusters appear in worked rural fields, stay at least 31 m from the
railway, avoid stations, tunnels and bridges, and return identically after chunk streaming.
Meadow Junction and Northbridge deliberately have broad city envelopes; generated building scale
and footprint-aware setbacks keep their apartments and warehouses clear of the railway and parallel
road. The densest chunks add a sparse, alternating second layer of stepped towers 92-188 m from
the railway, which stays visible above the road frontage without filling the active stream with a
full city simulation. Both endpoints use a terrain apron and distant low-cost skyline beyond the closed route datum,
so a side-window view remains grounded while the centred train dwells and reverses. The
whole corridor is double-track and electrified; the route owns the secondary-line offset used by
rails, AI trains, contact wire and line-side masts, so its island-platform widening cannot leave a
pole inside a running line. Streamed fencing, signals, protected level crossings with live
distance-driven barriers and alternating road-warning lamps responding to either passenger or AI
trains, named
platform signs, a level 20 cm-gap platform edge with coping, warning line and tactile band, a live
approach/boarding display at the active station, direction-facing speed
boards sourced from the same per-segment limits as the simulation, one-kilometre numbered route
posts readable from the adjacent passenger windows, major-station sidings/turnouts,
sparse high-voltage corridors and substations, tunnel portals, repeated wall/ceiling lighting,
raised evacuation walks, expansion joints, signed emergency niches with equipment cabinets, and
bridge decks with open safety rails make the railway legible without blocking passenger views.
The route model distinguishes the adjacent Lakehurst shore, broad river/estuary crossing and
wooded dry-valley viaduct. The river has deterministic surface glints and raised vegetated banks,
while the viaduct exposes contoured terrain instead of water. Both decks follow route curvature in
20 m structural sections. The river crossing uses dark steel fascia and broad blade piers; the dry
viaduct uses a pale concrete edge and paired slender shafts beneath wide caps. Every pier reaches
the independently sampled waterbed or valley terrain. Concrete breast/wing abutments and expansion
joints close each authored end. The river surface uses one route-owned absolute elevation across
50 m curved strips, so it remains level and reaches both bridge approaches independently of the
250 m streaming boundaries.
Terrain-anchored procedural features are tested against their exact route and lateral position, so
they stay out of tunnels, water, and the clear bridge corridor; road traffic never crosses an
unsupported span.
Sparse grade-separated cross roads add concrete overpasses with catenary clearance,
terrain-reaching ramps and supports. Deterministic exclusion zones keep them away from stations,
tunnels, and the two authored long crossings.

Block signals alternate facing direction and use live, service-aware aspects rather than random
decorative colors. The occupied block behind the train and a held departure signal show red,
approach control before a booked stop can show yellow, and clear blocks show green. Each mast has
three physical lens wells, a rear access ladder, numbered identity plate, foundation and relay
cabinet. Only the selected aspect face is dynamic; the complete maintained structure stays in the
static streamed scenery batch.

Station silhouettes follow the route character table rather than one repeated kit: the endpoints
use glazed terminal halls, Merefield and Stonebridge brick clock turrets, Hawthorn Park and
Eastmere pedestrian bridges, Greenford Vale and Pine Crossing compact wind shelters, Lakehurst and
Redwater waterfront edges, and Meadow Junction/Northbridge elevated concourses.
Major stations also carry marked platform clocks whose hour and minute hands advance from the
fictional 09:17 service-day start using simulated, not host, time.

At either terminal, doors open for the terminal dwell, destination information remains coherent,
the active direction reverses, doors close with warning audio, and the same double-ended consist
returns over the route. No turntable or implausible loop is required.
