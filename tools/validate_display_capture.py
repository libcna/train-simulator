#!/usr/bin/env python3
"""Validate the fixed route-52/car-2 screen fixture, including its cloud-state regression."""
import argparse
from pathlib import Path
import statistics
from validate_showcase import decode_png, ValidationError


def validate(image):
    if (image.width, image.height) != (1280, 720):
        raise ValidationError("Display fixture must be 1280x720")
    # These samples are inside the right cloud card, not the window frame or text.
    # A leaked texture-disable state makes that card a nearly uniform white rectangle.
    # ENV-040 raises this cumulus; the former y=245..280 rectangle is now clear sky.
    samples = [image.pixel(x,y)[0] for x in range(1100,1200,4) for y in range(190,230,4)]
    if statistics.pstdev(samples) < 5:
        raise ValidationError("Cloud coverage collapsed after the passenger text pass")
    # Three independent text baselines must survive atlas sampling and blending.
    for top, bottom in [(224,240),(244,254),(257,270)]:
        lit = sum(1 for y in range(top,bottom) for x in range(535,710)
                  if min(image.pixel(x,y)[:3]) > 95)
        if lit < 35:
            raise ValidationError(f"Missing passenger screen row at y={top}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    try:
        validate(decode_png(args.capture))
    except (OSError, ValidationError) as error:
        raise SystemExit(str(error))
    print("Passenger display rows and cloud coverage verified")


if __name__ == "__main__":
    main()
