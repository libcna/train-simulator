# Passenger and conductor simulation

The passenger system aims for readable variety without a heavyweight navigation server. It is
deterministic from the route seed and maintains explicit seat ownership, so two characters—or the
player and a character—cannot occupy one seat even through long station/reversal cycles.

Regular passengers choose among seated, conversational, standing, walking, looking outside,
visiting the toilet, and waiting in a vestibule. Activity timers and seeded choices keep changes irregular. Targets are
laid along the central aisle through all six cars; the narrow topology avoids expensive general
navmesh queries while matching the actual passenger-accessible space. Seat selection considers
car preference and availability. Free-seat and socially attractive-seat candidates use two
capacity-retaining scratch buffers, so long-running activity changes do not repeatedly allocate.
Changing seats reserves the destination, releases the old seat, walks the character along the
aisle, and then eases them laterally into the row before the seated pose begins. The player and
other passengers see that reservation as unavailable, eliminating both visible seat-to-seat
teleports and late ownership conflicts. A window-seat move also reserves the inner access position
until the crossing passenger sits; an already occupied inner seat makes that window place
temporarily unreachable for both NPC selection and the player's context prompt.
On departure from a seat, the old cushion and any inner access place remain reserved while the
avatar eases laterally into its aisle lane. Only then does longitudinal path following begin, so
standing up no longer teleports a character through the seat row or allows another actor to enter
the same space halfway through the motion.
The invariant checker uses a fixed bounded occupancy array, preserving its `noexcept` contract
even under memory pressure. Walkers select one of two subtle aisle lanes and normally yield when
their next step would reduce separation below 0.62 m, preventing obvious overlap and crowd clumping
while keeping the system inexpensive. Opposing lanes permit a tighter 0.52 m shoulder-to-shoulder
pass; both CNA avatars turn 0.42 radians as they meet, so the visual pose matches the narrow-carriage
clearance instead of letting two forward-facing torsos intersect. If the other lane has a clear
complete next step, a blocked regular passenger changes lanes and continues rather than forming an
indefinite face-to-face queue. The
lane-change count is exposed in F3 diagnostics and automated runtime reports. The live player
position participates in the same forward-step check. Aisle walkers, door-exchange actors, and the
conductor stop 1.15 m away when approaching, instead of walking through the player or crowding the
first-person camera. NPC-to-NPC clearance is handled independently by the normal and angled-pass
limits above, so changing player comfort distance cannot deadlock the aisle.
An unseated aisle actor within 1.30 m also eases from its ordinary lane to a shoulder-clearance
position on the side away from the player. This leaves a visible route for the player to take the
opposite side instead of turning a conductor inspection into an impassable wall. The actor returns
gradually to its normal lane after the player passes, with both motions using the same actor
clearance checks. Diagnostics count each newly initiated player-yield response.

The initial timers are deterministic residual activity times rather than a fresh minimum-duration
idle for every rider. The first minute therefore looks like a continuing journey instead of a
synchronized population that all begins to move after the same quiet threshold. A bounded share
of later choices lets a rider finish the real source-seat exit and remain beside that row briefly.
The ordinary aisle collision lane remains authoritative throughout.

Conversational idles are paired: a speaker is valid only when the adjacent seat in the same row and
side is occupied by another regular passenger. Both actors enter the state together and share its
remaining duration; if either leaves, the other returns to a normal seated idle. This keeps spatial
murmur attached to a visually plausible conversation rather than a lone passenger.
Talking avatars add a restrained identity-phased head and single-arm gesture over the shared seated
pose, while the independently selected CNA idle continues to animate the rest of the upper body.
Settled standing riders can also select the nearest unclaimed normally seated passenger in the
same car within conversational distance. The standing actor turns its body, both participants
derive filtered eye contact from their actual positions, and only the standing actor adds a quiet
single-arm gesture. Seat-exit transfers and still-moving vestibule actors are excluded, and the
pair is presentation-only: it cannot weaken seat ownership, personal-space, or path reservations.
`npc_social` counts visible participants in both seated and standing conversations.

Toilet visits reserve the compartment for one passenger, route the actor through the real doorway,
close the door for privacy while occupied, and reopen it for exit. Entering actors lock the player
out while the visibly open door remains usable by the AI, and the player's presence prevents a new
AI reservation. The same state exposes an occupied prompt to player interaction, so the visual
door, collision boundary, and ownership rules cannot disagree.

During the coasting/braking approach, two passengers with clear seat-row access release their real
seats, ease laterally into the aisle, and walk to distinct vestibules in their current cars. They
wait inside while the doors remain interlocked, then move sideways only after the real platform
side has spent 1.35 seconds opening and a conservative 1.45-second passage-release threshold has
elapsed. Boarding actors use the same gate. A direct QA start at a station retains a bounded
fallback so the exchange is still visible without a preceding approach. Two persistent offboard identities walk in from the
platform; completed alighters replenish that same reserve for a later stop instead of immediately
turning around or making a seated onboard actor disappear. This keeps storage and renderer demand
bounded while preserving a real population exchange. Actors prefer distinct car doorways. If a
dense saloon occupies all remaining apertures, one arrival may use the opposite half of an
alighter's 1.56 m double doorway with a tested 0.66 m longitudinal separation. During station duty
the conductor reserves a different doorway before either flow is assigned. The normal exchange is
two out/two in; one bounded recovery arrival can restore the two-identity reserve after a shortened
earlier dwell. Sideways entry,
seat-row transitions, and accelerated-dwell recovery all reuse the same actor-clearance query. If
doors close during an incomplete exchange, an arrival returns to the bounded offboard pool while
the interrupted departure walks back toward its still-reserved original seat. If a development
dwell or extreme time scale advances past a stop before a preparing passenger can reach the
vestibule, that passenger immediately reverses toward the still-reserved seat (or safely resumes an
onboard walk if the seat-row reservation was already released). NPCs never authorize player egress.
The live exchange-actor count is exposed in F3 and
`FINAL_STATE` diagnostics alongside the last launched participant and offboard counts, so station
behavior can be verified in a real renderer run without depending on how many actors finished
before capture.

The conductor has a distinct role and repeatedly traverses the full accessible consist, pausing
at end vestibules between rounds. During a round, a seeded bounded cooldown makes the conductor
pause briefly beside an occupied row with a dedicated rounded handheld terminal before continuing;
its screen, status strip, keys, and wrist loop are visible at close range. The inspection uses a
two-arm device pose and downward gaze unless the player is being acknowledged; while walking the
same terminal clips at the conductor's side. Unlike a
per-frame chance, the cooldown cannot silently omit checks for an arbitrarily long period. While
station doors are open, the conductor temporarily attends
the nearest door on the real platform side and then resumes the interrupted round. The driver
remains in the physically separated cab. The conductor owns a reserved CNA visual selected with a
catalog-native service-blue cap and top (and therefore CNA's matching hair-under-hat mesh), plus a
project-generated handheld device/satchel. These distinguish the role without modifying the CNA
avatar implementation. Rendering stages the nearest 6/9/12 varied CNA avatar descriptions on
Low/Medium/High, assigns at most one visible character to each renderer per frame, prioritizes the
conductor, and culls the crowd by distance. A persistent `(passenger id, appearance seed)`
assignment receives a four-metre selection hysteresis while the actor remains inside the 28 m
visibility window. Small distance-order changes therefore retain the same face and clothing, while
a substantially closer actor can still enter the bounded pool. Every passenger keeps independent
state whether visible or not. Seated actors use CNA's public bone-transform path to bend hips and
knees while retaining their independently selected upper-body idle. A 0.55-second per-visual blend
eases hip/knee angles, body height, and scale between locomotion and the seated pose instead of
switching the skeleton in one frame. Moving actors use the same path for an identity-seeded hip,
knee, ankle, toe, shoulder, and vertical gait layered over the idle, preventing the visibly sliding
rigid pose that a translation-only approximation would produce. Phase advances from measured
train-local distance instead of elapsed render time. A blocked navigator therefore plants and
blends out rather than walking on the spot, render-only avatar warm-up cannot move its feet, and a
discontinuous spawn/teleport sample cannot inject a giant stride. Gait amplitude eases in over
0.18 seconds and out over 0.24 seconds, and persistent visuals turn through the shortest angular
path at a bounded rate. Stopping and direction changes therefore no longer snap the complete
skeleton or yaw in one frame.

Standing navigation remains authoritative from the first simulation update, but a source-seat
reservation identifies the physical place the character is leaving. During the 0.55-second exit
blend, the rendered pelvis is weighted toward that seat and converges on the navigation position
as the skeleton straightens. The facing target turns toward the corresponding aisle lane and a
small lower-back curve supplies the middle of the weight transfer. This removes the former
sideways seated slide without delaying collision, freeing the seat early, or changing narrow-aisle
path timing. The same bounded curve supplies a restrained forward weight shift while a returning
actor settles into the seated pose. Pure placement tests cover both endpoints, the midpoint,
clamping, the inverse sit-down weight, and the unaffected ordinary-walking path;
`npc_seat_transitions` reports currently visible transfers.

Each persistent visual also filters bounded head yaw and pitch at 0.85 rad/s. Talking riders derive
their local look direction from the actual adjacent partner seat, window-watchers scan the passing
view, and other stationary riders use a slow appearance-phased glance. A deterministic quarter of
neutral seated riders adds a two-handed reading/phone-like lap pose and downward gaze. These are
render layers over authoritative AI state rather than new simulation states; runtime gaze/reading
counters and the High avatar smoke protect their activation.

A deterministic subset of stationary passengers, and the conductor, acknowledges the player from
0.70-2.55 m when not talking or watching the landscape. Head motion retains the normal filter and
the upper torso follows only 18% of yaw and 8% of pitch, so attention reads without making the
whole carriage stare. Runtime acknowledgement and conductor-device counters protect the feature.

Civilian appearance generation considers at most eight deterministic CNA avatar descriptions per
visual. A project-owned score rejects the catalog's bright broad bucket/workwear-like combination,
but retains restrained caps, beanies, natural hair, and clothing variety. This is a bounded
read-only selection over CNA's documented description bytes, not a dependency patch; the selected
civilian pool size appears in runtime diagnostics.

The audio layer periodically selects a nearby regular passenger as a conversational source,
preferring an actor in the seated talking state. Short spatialized murmur bursts use
identity-derived pitch variation and randomized quiet gaps; boarding, alighting, and all phases of
a toilet visit are excluded so the sound remains plausible and unobtrusive.

Stations reserve 2/4/6 distinct CNA avatar visuals for the Low/Medium/High window-visible platform
crowd, so no renderer or appearance is reused within one frame. The pool is constructed only to the
active quality budget and expands safely if quality is raised. These decorative people exist only
near the current station, are distance-culled, cast the same contact shadows where the profile
enables them, and include one high-visibility employee. Alternating waiting passengers have a
shared low-cost wheeled case with an extended handle and luggage tag, grounding the avatar group in
the journey context without allocating a unique prop mesh per person. They deliberately do
not join the persistent passenger state machine: actual boarding and alighting remain represented
by the authoritative onboard population and its seat ownership.

The test suite advances the system for 200,000 updates (20,000 simulated seconds) and repeatedly
checks seat references, unique ownership, valid cars, and population consistency. Separate tests
verify conductor traversal, onboard inspection pauses and platform-door duty, authoritative player reservations, and
platform-side station exchange, including a no-teleport seat-to-vestibule approach. The long soak
also requires the offboard reserve to return to exactly two and retains a realistic seated
population of more than twenty after one hundred shortened station exchanges. A 40,000-step dense-flow regression
samples every unseated onboard
pair and rejects body separation below 0.50 m. A continuous-walk test also follows one passenger
from reservation to occupancy and rejects any single-update position jump.
A focused deterministic regression requires an ordinary passenger to complete a source-seat exit,
settle in the 0.28 m aisle lane, and retain all population invariants during the standing idle.
A coupled A-B-A-B soak feeds the actual service phase, direction, upcoming platform side, and
door state into the passenger system for three complete terminal reversals. It requires at least
28 full four-person exchanges while checking the active driver cab, offboard reserve, seat
ownership, and streaming bound.
A real OpenGL smoke additionally proves that a development-scaled dwell cannot close before one
fully open passenger update, then closes shortly after all four exchange actors launch and
requires zero actors left in a doorway and exactly two identities restored to the offboard reserve.
Developer station teleports settle any active doorway flow, restore the waiting reserve, and arm a
new exchange on the newly selected platform side; a deterministic regression covers the complete
negative-to-positive-side handover.
