#!/usr/bin/env python3
"""Render a fixed street view and guard the opt-in facade effect's depth integration."""

import argparse
import os
from pathlib import Path
import subprocess

from validate_showcase import decode_png


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--quality", type=int, choices=(1, 2), default=1)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    environment = dict(os.environ)
    environment.setdefault("SDL_VIDEODRIVER", "offscreen")
    environment.setdefault("SDL_AUDIODRIVER", "dummy")
    environment.setdefault("LP_NUM_THREADS", "4")
    images = []
    for mode, flag in enumerate((None, "--facade-control", "--linear-facades")):
        name = ("stock", "control", "linear")[mode]
        capture = args.output / (name + ".png")
        command = [str(args.executable.resolve()), "--frames", "3", "--route-km", "0.783",
                   "--time-scale", "0.000001", "--left-window-view", "--quality", str(args.quality),
                   "--no-audio", "--no-avatars", "--report-state", "--screenshot", str(capture),
                   "--config", str(Path(__file__).resolve().parent.parent / "tests/facade-comparison.cfg")]
        if flag:
            command.append(flag)
        result = subprocess.run(command, env=environment, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=40)
        (args.output / (name + ".log")).write_text(result.stdout, encoding="utf-8")
        if result.returncode or f"facade_lighting={mode} facade_shader_draws={2 if mode else 0}" not in result.stdout:
            raise RuntimeError(f"{name}: facade batch execution failed; see retained log")
        images.append(decode_png(capture))
    if any((image.width, image.height) != (1280, 720) for image in images):
        raise RuntimeError("Unexpected comparison dimensions")
    stock, control, linear = images
    total = stock.width * stock.height
    error = major = interior_major = changed = 0
    for offset in range(0, len(stock.rgba), 4):
        delta = [abs(stock.rgba[offset+c] - control.rgba[offset+c]) for c in range(3)]
        error += sum(delta)
        if max(delta) > 20:
            major += 1
            pixel = offset // 4
            x, y = pixel % stock.width, pixel // stock.width
            # Different effect paths can resolve a silhouette one pixel apart. Keep a
            # stricter interior-error limit rather than treating every shifted edge as
            # a lost wall. Mean error below still includes every original pixel.
            matched = False
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nx, ny = x + dx, y + dy
                    if not (0 <= nx < stock.width and 0 <= ny < stock.height):
                        continue
                    other = (ny * stock.width + nx) * 4
                    if max(abs(stock.rgba[offset+c] - control.rgba[other+c]) for c in range(3)) <= 20:
                        matched = True
            interior_major += not matched
        changed += max(abs(linear.rgba[offset+c] - control.rgba[offset+c]) for c in range(3)) > 20
    mean = error / (total * 3)
    print(f"Facade comparison: quality={args.quality} control_mean={mean:.4f} "
          f"control_major={major/total:.6f} control_interior={interior_major/total:.6f} "
          f"linear_changed={changed/total:.6f}")
    # Not a certified pixel-identical BasicEffect replacement (sampling/edge differences).
    # The missing clip-depth conversion raises mean error above 1 and hides the
    # linear-light change. Test both agreement and a substantial visible wall change.
    if (mean > 0.25 or major / total > 0.002 or interior_major / total > 0.0005
            or not 0.05 < changed / total < 0.20):
        raise RuntimeError("Facade depth/lighting comparison failed; inspect retained images")
    print("FACADE_LIGHTING_PASS")


if __name__ == "__main__":
    main()
