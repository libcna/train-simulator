# Audio system

The runtime mix is implemented with CNA `SoundEffect` and `SoundEffectInstance` objects. It does
not require text-to-speech, downloads, or an internet connection while playing.

- Rail rolling volume and pitch follow speed.
- Traction effort follows the simulation's measured positive longitudinal acceleration and tracks
  speed in pitch; braking effort follows measured negative acceleration. Both ease between levels
  in real time instead of switching abruptly with a service-phase label, so gentle coasting remains
  audibly distinct from the 0.78 m/s2 service brake.
- Wind rises nonlinearly at higher speed.
- A quiet interior HVAC loop remains audible at a standstill, ducks with announcements, and eases
  slightly beneath the platform ambience while the passenger doors are open.
- Rail-joint texture follows speed, with braking and curve-squeal layers mixed by service phase and
  sampled route curvature.
- A spatialized gangway-rattle layer follows the nearest inter-car connection and is attenuated by
  both player distance and train speed.
- Sensor transitions at the nearest gangway emit a short spatialized pneumatic glide with distinct
  opening and closing pitches. NPCs and the player drive the same trigger used by the visible doors.
- The accessible-toilet door has its own quiet spatialized hinge, mechanism, and latch layer. The
  authoritative combined player/NPC door state triggers it, and opening/closing pitch variation
  follows the same animated door seen from the saloon.
- Two alternating, softly pitched carpet footfalls follow actual successful player displacement.
  A separate pair with a restrained panel ring is selected in resilient-floor vestibules,
  gangway bridge plates, and the accessible toilet. Surface lookup consumes the same authored
  interior positions as collision; pushing into a seat, wall, or another passenger remains silent
  instead of producing fake steps.
- Tunnel rumble and bridge resonance cross-fade independently using the authoritative authored
  route zones. Paired OpenGL/dummy-mixer smokes start inside the long tunnel and river crossing,
  then require both the corresponding streamed geometry and only the correct acoustic layer to be
  active; a valid route flag or loaded WAV alone cannot satisfy them.
- A passing AI train emits a short spatial pressure, wheel, and wind event on the adjacent track.
  Its trigger consumes the same deterministic moving-train position helper as rendering, so the
  visible articulated consist and audible pass cannot drift apart. It also consumes the route's
  widened parallel-track alignment at island platforms rather than a duplicated fixed offset. The
  1.9 km visible traversal resets only during an explicit silent ten-second interval. The OpenGL
  smoke requires the pass-by event, all three rendered external cars and both driving-cab ends,
  rather than accepting an off-screen sound or a partially clipped consist.
- A quiet synthesized electromechanical warning bell is spatialized at the nearest active level
  crossing. It consumes the same player-and-AI closure calculation as the two visual barriers, so
  an opposing service cannot lower the road protection silently or let audio outlive raised booms.
- Passenger murmur plays as short, irregular conversational bursts from a selected nearby regular
  passenger through CNA 3D audio. Four deterministic non-verbal voice textures provide different
  registers and cadences; speaker identity selects a texture and subtly changes its pitch. Active
  boarding and toilet actors are excluded, and a randomized quiet interval prevents a constant
  wall of voices. A visually paired conversation is preferred whenever one is close enough.
  Runtime diagnostics count successfully started murmur instances. The offscreen audio smoke waits
  through the same natural eight-second opening cooldown used in normal play and requires a nearby
  simulated passenger to start one; no test-only trigger bypasses selection or spatialization.
- A quiet synthetic platform bed cross-fades in while station-side doors are open and fades out
  after door closure, adding distant ventilation, city wash, and sparse bird-like detail.
- Door warning, opening, and closing one-shots follow service door transitions.
  Their initial edge detector is seeded from the actual service state, so a moving `--route-km`
  start cannot emit a synthetic warning/closure event for doors that were never open.
- A direction-aware welcome announcement plays after startup. If a resumed or diagnostic moving
  service is already inside the 1.65 km station-approach envelope, the operationally useful arrival
  message takes priority instead of waiting behind the full welcome recording. Two station
  phrasings alternate on successive legs, and the endpoint-specific terminal message names the new
  destination before reversal. Every announcement is preceded by a short project-synthesized
  two-note onboard PA chime. Speech is queued until the chime finishes, so neither two voices nor a
  new chime and an active voice can overlap; captions appear only when the spoken clip actually
  begins.
- Runtime diagnostics count successfully started announcement instances. A dummy-mixer OpenGL
  smoke waits through the normal one-second chime-to-speech sequence and requires the last actually
  started welcome clip to name the active destination. The active-caption diagnostic remains
  separate and becomes empty when speech ends, matching the player-facing overlay. The
  return-direction variant proves the Alderwick-bound selection independently. A moving-service
  smoke enters the real 1.65 km approach trigger and requires the most recently selected queued,
  active, or completed clip to name the authoritative next station. This keeps route-selection
  coverage deterministic even when a fast or heavily loaded software renderer reaches its frame
  limit on a different side of the real-time chime boundary.
- Terminal announcements have their own successful-playback counter. The serialized long audio
  smoke arrives at Westhaven under accelerated service time, waits in real audio time for the
  approach message to finish, and requires the queued final-stop message to begin after the service
  reverses toward Alderwick. The same run also covers the natural passenger-murmur cooldown.
- Every played announcement publishes its exact English script to a synchronized on-screen caption
  panel. Captions are enabled by default and can be toggled in the comfort menu or configuration.
  Pure route-aware script functions are shared by playback and deterministic tests, which cover
  both endpoint directions and both phrasing variants for every station.
- While speech is active, rolling, traction, wind and local ambience ease down by 30 percent and
  nearby passenger voices by 82 percent, then recover more slowly after the final word. Door
  warnings remain unducked and exterior crossing warnings retain most of their level, preserving
  safety cues without forcing announcements to compete with the full interior sound bed.

Master, announcement, passenger, and train mix levels come from the configuration system. The
pause menu can adjust the same mix levels immediately and saves them to the selected XDG or
explicit user-preference file.
Opening that menu pauses every owned CNA audio instance along with the service and passenger
simulation. Resuming continues looped beds, announcements, spatial one-shots, and their remaining
playback positions rather than restarting or letting them expire while the world is frozen. A
dummy-mixer application smoke requires zero playing instances, at least one paused instance, and
zero route movement while the menu is open. The full audio interaction smoke additionally cycles
the live loop set through pause and resume, requiring an exact playing-instance count before and
after the transition.
The `--no-audio` option completely skips asset loading for silent/test environments.

## Reproducible generation

`tools/generate_audio.py` deterministically synthesizes every non-speech sound from waveforms and
seeded noise. With the optional Piper arguments it also renders the announcement script:

```bash
python3 tools/generate_audio.py \
  --piper /path/to/piper/piper \
  --piper-model /path/to/en_US-ljspeech-medium.onnx
```

The distributed announcements were generated with archived Piper release `2023.11.14-2` and the
`en_US-ljspeech-medium` voice at piper-voices commit
`217ddc79818708b078d0d14a8fae9608b9d77141`. The voice model is a generation-time input only and
is not committed. Exact provenance and hashes are in `docs/ASSETS.md`.

The committed WAV files are mono PCM: generated ambience is 24 kHz, and Piper speech is 22.05 kHz.
Keeping them uncompressed avoids runtime codec dependencies and the complete audio set remains
small.

The committed non-speech set can be checked without touching it or retaining a second asset cache:

```bash
python3 tools/generate_audio.py --verify-existing assets/audio
```

The verifier regenerates all 27 project-synthesized WAV files in an automatically removed
temporary directory and requires byte-for-byte equality. CTest runs this check whenever project
tests are enabled; Piper announcements retain the separately recorded model/tool provenance above.
