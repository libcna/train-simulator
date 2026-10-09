#!/usr/bin/env python3
"""Generate Train Simulator's redistributable WAV assets.

The ambience is synthesized from deterministic waveforms.  Announcements are
optionally rendered with a locally installed Piper binary and voice model; the
model itself is a development input and is not copied into the repository.
"""

from __future__ import annotations

import argparse
import filecmp
import math
import random
import struct
import subprocess
import tempfile
import wave
from pathlib import Path


SAMPLE_RATE = 24_000
STATIONS = [
    "Alderwick Central",
    "Merefield",
    "Hawthorn Park",
    "Greenford Vale",
    "Lakehurst",
    "Pine Crossing",
    "Stonebridge",
    "Meadow Junction",
    "Northbridge",
    "Redwater",
    "Eastmere",
    "Westhaven Central",
]


def clamp(value: float) -> float:
    return max(-1.0, min(1.0, value))


def write_wave(path: Path, seconds: float, sample) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = bytearray()
    for index in range(round(seconds * SAMPLE_RATE)):
        value = int(clamp(sample(index / SAMPLE_RATE)) * 32_767)
        frames.extend(struct.pack("<h", value))
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(frames)


def make_ambience(output: Path) -> None:
    rng = random.Random(0x4E564558)
    noise = [rng.uniform(-1.0, 1.0) for _ in range(SAMPLE_RATE * 18)]

    def n(t: float, rate: float = 1.0) -> float:
        return noise[int(t * SAMPLE_RATE * rate) % len(noise)]

    write_wave(
        output / "rolling.wav",
        8.0,
        lambda t: 0.20 * math.sin(2 * math.pi * 57 * t)
        + 0.10 * math.sin(2 * math.pi * 91 * t)
        + 0.07 * n(t),
    )
    write_wave(
        output / "traction.wav",
        8.0,
        lambda t: 0.18 * math.sin(2 * math.pi * (116 + 5 * math.sin(t * 0.7)) * t)
        + 0.06 * math.sin(2 * math.pi * 232 * t),
    )
    write_wave(
        output / "wind.wav",
        8.0,
        lambda t: 0.06 * n(t, 0.31) + 0.035 * n(t, 0.07),
    )
    write_wave(
        output / "tunnel_rumble.wav",
        8.0,
        lambda t: 0.18 * math.sin(2 * math.pi * 43 * t)
        + 0.09 * math.sin(2 * math.pi * 67 * t)
        + 0.04 * n(t, 0.4),
    )
    write_wave(
        output / "bridge_rumble.wav",
        8.0,
        lambda t: 0.15 * math.sin(2 * math.pi * 36 * t)
        + 0.08 * math.sin(2 * math.pi * 54 * t)
        + 0.035 * n(t, 0.55),
    )

    def passing_train(t: float) -> float:
        # A short exterior pressure/wheel event. Spatial playback supplies direction; the bell
        # envelope supplies the rapid approach and decay heard through sealed passenger windows.
        centre = 1.55
        proximity = math.exp(-((t - centre) / 0.62) ** 2)
        pressure = math.sin(2 * math.pi * (48 + 9 * math.sin(t * 2.1)) * t)
        wheel_phase = t % 0.19
        wheel_impact = math.exp(-wheel_phase * 37.0)
        return proximity * (
            0.13 * n(t + 3.4, 0.82)
            + 0.10 * pressure
            + 0.055 * wheel_impact * math.sin(2 * math.pi * 94 * t)
        )

    write_wave(output / "passing_train.wav", 3.2, passing_train)

    def crossing_warning(t: float) -> float:
        # Four damped electromechanical bell strikes form a seamless 2.2-second loop. Runtime
        # attenuation and panning place the source at the nearest active roadside warning post.
        local = t % 0.55
        envelope = math.exp(-local * 8.5)
        carrier = math.sin(2 * math.pi * 735 * local)
        overtone = math.sin(2 * math.pi * 1_470 * local + 0.35)
        strike = math.exp(-local * 46.0) * n(t + 1.73, 0.9)
        return envelope * (0.15 * carrier + 0.055 * overtone + 0.035 * strike)

    write_wave(output / "crossing_warning.wav", 2.2, crossing_warning)
    write_wave(
        output / "braking.wav",
        8.0,
        lambda t: (0.075 + 0.025 * math.sin(2 * math.pi * 1.7 * t))
        * math.sin(2 * math.pi * (640 + 34 * math.sin(t * 0.43)) * t)
        + 0.028 * n(t, 0.8),
    )
    write_wave(
        output / "curve_squeal.wav",
        8.0,
        lambda t: 0.075 * math.sin(2 * math.pi * (910 + 21 * math.sin(t * 0.7)) * t)
        + 0.035 * math.sin(2 * math.pi * 1_365 * t),
    )

    def rail_texture(t: float) -> float:
        phase = t % 0.64
        impact = math.exp(-phase * 48.0)
        return impact * (0.16 * math.sin(2 * math.pi * 82 * t) + 0.055 * n(t, 1.2))

    write_wave(output / "rail_texture.wav", 8.0, rail_texture)

    def gangway_rattle(t: float) -> float:
        phase = t % 0.47
        impact = math.exp(-phase * 31.0)
        return impact * (
            0.13 * math.sin(2 * math.pi * 74 * t) + 0.075 * n(t, 1.35)
        ) + 0.014 * n(t, 0.33)

    write_wave(output / "gangway_rattle.wav", 8.0, gangway_rattle)

    def gangway_door(t: float) -> float:
        duration = 0.72
        envelope = math.sin(math.pi * min(1.0, t / duration)) ** 2
        pneumatic = 0.075 * n(t, 0.24) + 0.045 * math.sin(
            2 * math.pi * (235 - 105 * t / duration) * t
        )
        latch = 0.12 * math.exp(-t * 42.0) * math.sin(2 * math.pi * 178 * t)
        return envelope * pneumatic + latch

    write_wave(output / "gangway_door.wav", 0.72, gangway_door)

    def toilet_door(t: float) -> float:
        # A compact hinged-door movement with a soft composite-panel creak and a late latch.
        # Runtime pitch distinguishes opening from closing while preserving one small asset.
        duration = 0.86
        travel = math.sin(math.pi * min(1.0, t / duration)) ** 2
        creak = 0.050 * math.sin(2 * math.pi * (118 + 22 * math.sin(t * 7.0)) * t)
        mechanism = 0.038 * n(t + 4.17, 0.22)
        latch_time = t - 0.72
        latch = (
            0.15
            * math.exp(-latch_time * 42.0)
            * math.sin(2 * math.pi * 205 * latch_time)
            if latch_time >= 0.0
            else 0.0
        )
        return travel * (creak + mechanism) + latch

    write_wave(output / "toilet_door.wav", 0.86, toilet_door)

    for variant in range(2):
        def interior_footstep(t: float, variant: int = variant) -> float:
            impact = math.exp(-t * (19.0 + variant * 2.0))
            low = math.sin(2 * math.pi * (72 + variant * 9) * t)
            scuff_time = t - (0.065 + variant * 0.012)
            scuff = math.exp(-(scuff_time * scuff_time) / 0.0024) * n(
                t + variant * 1.37, 0.43 + variant * 0.05
            )
            return 0.17 * impact * low + 0.055 * scuff

        write_wave(output / f"footstep_carpet_{variant + 1:02d}.wav", 0.32,
                   interior_footstep)

    for variant in range(2):
        def vestibule_footstep(t: float, variant: int = variant) -> float:
            # A firmer heel strike and short high-frequency floor-panel ring distinguish the
            # resilient vestibule/gangway surface without turning each step into a loud clang.
            impact = math.exp(-t * (24.0 + variant * 2.5))
            heel = math.sin(2 * math.pi * (96 + variant * 11) * t)
            ring = math.exp(-t * (15.5 + variant)) * (
                math.sin(2 * math.pi * (312 + variant * 37) * t)
                + 0.42 * math.sin(2 * math.pi * (486 + variant * 29) * t + 0.31)
            )
            scuff_time = t - (0.052 + variant * 0.010)
            scuff = math.exp(-(scuff_time * scuff_time) / 0.0015) * n(
                t + 7.91 + variant * 1.19, 0.67 + variant * 0.06
            )
            return 0.14 * impact * heel + 0.050 * ring + 0.038 * scuff

        write_wave(output / f"footstep_vestibule_{variant + 1:02d}.wav", 0.30,
                   vestibule_footstep)

    # Four non-verbal voice textures made from short formant-like syllables. Each variant has a
    # distinct register, cadence, and harmonic character; none contains intelligible language.
    # The first syllables start promptly because playback deliberately uses only a short excerpt.
    murmur_names = [
        "passenger_murmur.wav",
        "passenger_murmur_01.wav",
        "passenger_murmur_02.wav",
        "passenger_murmur_03.wav",
    ]
    register_ranges = [(84.0, 128.0), (102.0, 154.0), (122.0, 182.0), (142.0, 208.0)]
    for variant, (name, register) in enumerate(zip(murmur_names, register_ranges)):
        voice_rng = random.Random(0x535045454348 + variant * 0x101)
        syllables = []
        for syllable in range(24):
            if syllable == 0:
                start = 0.04 + variant * 0.025
            elif syllable == 1:
                start = 0.34 + variant * 0.04
            else:
                start = voice_rng.uniform(0.55, 7.65)
            duration = voice_rng.uniform(0.18, 0.64)
            fundamental = voice_rng.uniform(*register)
            syllables.append(
                (start, duration, fundamental, voice_rng.uniform(0.2, 2.4))
            )

        second_ratio = 1.92 + variant * 0.09
        third_ratio = 3.05 + variant * 0.13

        def murmur(t: float) -> float:
            value = 0.0
            for start, duration, frequency, character in syllables:
                local = t - start
                if 0.0 <= local < duration:
                    envelope = math.sin(math.pi * local / duration) ** 2
                    wobble = 1.0 + 0.021 * math.sin(local * (17.0 + variant) + character)
                    value += envelope * (
                        0.052 * math.sin(2 * math.pi * frequency * wobble * local + character)
                        + 0.026 * math.sin(2 * math.pi * frequency * second_ratio * local)
                        + 0.016 * math.sin(2 * math.pi * frequency * third_ratio * local)
                    )
            return value + 0.010 * n(t + variant * 2.17, 0.18 + variant * 0.015)

        write_wave(output / name, 8.0, murmur)

    def station_ambience(t: float) -> float:
        # A restrained platform bed: distant ventilation/city wash plus a few synthetic bird-like
        # chirps. It stays non-verbal so announcements remain clear and the loop suits every stop.
        value = 0.020 * n(t, 0.11) + 0.012 * math.sin(2 * math.pi * 47 * t)
        for start in (1.4, 5.1, 8.8):
            local = t - start
            if 0.0 <= local < 0.42:
                envelope = math.sin(math.pi * local / 0.42) ** 2
                sweep = 1_420 + 380 * local / 0.42
                value += 0.030 * envelope * math.sin(2 * math.pi * sweep * local)
        return value

    write_wave(output / "station_ambience.wav", 12.0, station_ambience)

    def interior_hvac(t: float) -> float:
        # A quiet sealed-car ventilation bed. Integer-period fan harmonics keep the eight-second
        # loop stable, while cross-faded deterministic noise prevents a sterile sine-wave hum
        # without leaving a discontinuity at the loop boundary.
        fan_wander = 1.0 + 0.012 * math.sin(2 * math.pi * 0.25 * t)
        noise_blend = 0.5 - 0.5 * math.cos(math.pi * t / 8.0)
        fan_noise = (
            (1.0 - noise_blend) * n(t + 6.7, 0.08)
            + noise_blend * n(t - 8.0 + 6.7, 0.08)
        )
        return (
            0.058 * math.sin(2 * math.pi * 60 * fan_wander * t)
            + 0.026 * math.sin(2 * math.pi * 120 * t + 0.21)
            + 0.014 * fan_noise
        )

    write_wave(output / "interior_hvac.wav", 8.0, interior_hvac)

    def announcement_chime(t: float) -> float:
        # A restrained two-note onboard PA cue. The project synthesizes it directly so it is
        # reproducible, license-clean, and does not imitate a branded railway operator's motif.
        value = 0.0
        for start, duration, frequency in ((0.03, 0.34, 659.25), (0.45, 0.38, 783.99)):
            local = t - start
            if 0.0 <= local < duration:
                envelope = math.sin(math.pi * local / duration) ** 2
                value += envelope * (
                    0.17 * math.sin(2 * math.pi * frequency * local)
                    + 0.045 * math.sin(2 * math.pi * frequency * 2.0 * local + 0.18)
                    + 0.018 * math.sin(2 * math.pi * frequency * 3.0 * local + 0.42)
                )
        return value

    write_wave(output / "announcement_chime.wav", 0.92, announcement_chime)
    write_wave(
        output / "door_warning.wav",
        2.4,
        lambda t: (0.20 * math.sin(2 * math.pi * 880 * t))
        if int(t / 0.30) % 2 == 0 and (t % 0.30) < 0.16
        else 0.0,
    )
    write_wave(
        output / "door_open.wav",
        1.35,
        lambda t: 0.12 * n(t, 0.15) * math.sin(math.pi * min(1.0, t / 1.35))
        + 0.09 * math.sin(2 * math.pi * (145 - 55 * t / 1.35) * t),
    )
    write_wave(
        output / "door_close.wav",
        1.55,
        lambda t: 0.11 * n(t, 0.18) * math.sin(math.pi * min(1.0, t / 1.55))
        + 0.08 * math.sin(2 * math.pi * (90 + 70 * t / 1.55) * t),
    )


def render_piper(piper: str, model: Path, output: Path, name: str, text: str) -> None:
    destination = output / name
    subprocess.run(
        [piper, "-m", str(model), "-f", str(destination)],
        input=text + "\n",
        text=True,
        check=True,
    )


def make_announcements(piper: str, model: Path, output: Path) -> None:
    render_piper(
        piper,
        model,
        output,
        "announcement_welcome.wav",
        "Welcome aboard the Northern Valley Express. This train is bound for Westhaven Central.",
    )
    render_piper(
        piper,
        model,
        output,
        "announcement_welcome_alderwick.wav",
        "Welcome aboard the Northern Valley Express. This train is bound for Alderwick Central.",
    )
    render_piper(
        piper,
        model,
        output,
        "announcement_reverse_to_alderwick.wav",
        "This is Westhaven Central, the final stop. Please remember to take all your belongings "
        "with you. This service will shortly continue to Alderwick Central.",
    )
    render_piper(
        piper,
        model,
        output,
        "announcement_reverse_to_westhaven.wav",
        "This is Alderwick Central, the final stop. Please remember to take all your belongings "
        "with you. This service will shortly continue to Westhaven Central.",
    )
    for index, station in enumerate(STATIONS):
        render_piper(
            piper,
            model,
            output,
            f"announcement_station_{index:02d}.wav",
            f"The next station is {station}. We will shortly be arriving at {station}. "
            "Please mind the gap between the train and the platform.",
        )
        render_piper(
            piper,
            model,
            output,
            f"announcement_station_{index:02d}_alternate.wav",
            f"We are now approaching {station}. Please make sure you have all your belongings "
            "and mind the gap when leaving the train.",
        )


def verify_ambience(reference: Path) -> None:
    if not reference.is_dir():
        raise SystemExit(f"Audio reference directory does not exist: {reference}")
    with tempfile.TemporaryDirectory(prefix="train-simulator-audio-") as temporary:
        generated = Path(temporary)
        make_ambience(generated)
        generated_files = sorted(generated.glob("*.wav"))
        mismatches = []
        for candidate in generated_files:
            expected = reference / candidate.name
            if not expected.is_file() or not filecmp.cmp(candidate, expected, shallow=False):
                mismatches.append(candidate.name)
        if mismatches:
            raise SystemExit(
                "Generated ambience differs from committed assets: " + ", ".join(mismatches)
            )
        print(f"Verified {len(generated_files)} deterministic ambience WAV files.")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=Path("assets/audio"))
    parser.add_argument("--piper", default="piper")
    parser.add_argument("--piper-model", type=Path)
    parser.add_argument(
        "--verify-existing",
        type=Path,
        help="regenerate ambience in a temporary directory and compare it byte-for-byte",
    )
    args = parser.parse_args()
    if args.verify_existing:
        if args.piper_model:
            parser.error("--verify-existing checks project synthesis and cannot use --piper-model")
        verify_ambience(args.verify_existing)
        return
    make_ambience(args.output)
    if args.piper_model:
        make_announcements(args.piper, args.piper_model, args.output)


if __name__ == "__main__":
    main()
