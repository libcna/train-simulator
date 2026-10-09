#!/usr/bin/env python3
"""Validate the dimensions and critical UI content of showcase PNG captures."""

from __future__ import annotations

import argparse
import binascii
from dataclasses import dataclass
from pathlib import Path
import struct
import sys
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class ValidationError(RuntimeError):
    pass


@dataclass(frozen=True)
class Image:
    width: int
    height: int
    rgba: bytes

    def pixel(self, x: int, y: int) -> tuple[int, int, int, int]:
        offset = (y * self.width + x) * 4
        return tuple(self.rgba[offset : offset + 4])  # type: ignore[return-value]


def paeth(a: int, b: int, c: int) -> int:
    estimate = a + b - c
    distance_a = abs(estimate - a)
    distance_b = abs(estimate - b)
    distance_c = abs(estimate - c)
    if distance_a <= distance_b and distance_a <= distance_c:
        return a
    if distance_b <= distance_c:
        return b
    return c


def decode_png(path: Path) -> Image:
    payload = path.read_bytes()
    if not payload.startswith(PNG_SIGNATURE):
        raise ValidationError(f"{path}: not a PNG file")

    position = len(PNG_SIGNATURE)
    width = height = 0
    compressed = bytearray()
    saw_end = False
    while position < len(payload):
        if position + 12 > len(payload):
            raise ValidationError(f"{path}: truncated PNG chunk")
        length = struct.unpack_from(">I", payload, position)[0]
        chunk_type = payload[position + 4 : position + 8]
        data_begin = position + 8
        data_end = data_begin + length
        crc_end = data_end + 4
        if crc_end > len(payload):
            raise ValidationError(f"{path}: truncated {chunk_type!r} chunk")
        chunk_data = payload[data_begin:data_end]
        stored_crc = struct.unpack_from(">I", payload, data_end)[0]
        actual_crc = binascii.crc32(chunk_type)
        actual_crc = binascii.crc32(chunk_data, actual_crc) & 0xFFFFFFFF
        if stored_crc != actual_crc:
            raise ValidationError(f"{path}: invalid {chunk_type!r} CRC")

        if chunk_type == b"IHDR":
            if length != 13:
                raise ValidationError(f"{path}: invalid IHDR length")
            width, height, bit_depth, color_type, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", chunk_data)
            )
            if (bit_depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                raise ValidationError(
                    f"{path}: expected non-interlaced 8-bit RGBA PNG, got "
                    f"depth={bit_depth} color={color_type} interlace={interlace}"
                )
        elif chunk_type == b"IDAT":
            compressed.extend(chunk_data)
        elif chunk_type == b"IEND":
            saw_end = True
            break
        position = crc_end

    if not saw_end or width <= 0 or height <= 0 or not compressed:
        raise ValidationError(f"{path}: incomplete PNG structure")
    if (width, height) != (1280, 720):
        raise ValidationError(f"{path}: expected 1280x720, got {width}x{height}")

    try:
        filtered = zlib.decompress(compressed)
    except zlib.error as error:
        raise ValidationError(f"{path}: corrupt compressed pixels: {error}") from error
    stride = width * 4
    if len(filtered) != (stride + 1) * height:
        raise ValidationError(f"{path}: unexpected decompressed pixel size")

    rgba = bytearray(stride * height)
    previous = bytearray(stride)
    source = 0
    for y in range(height):
        filter_type = filtered[source]
        source += 1
        encoded = filtered[source : source + stride]
        source += stride
        row = bytearray(stride)
        for index, value in enumerate(encoded):
            left = row[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = above
            elif filter_type == 3:
                predictor = (left + above) // 2
            elif filter_type == 4:
                predictor = paeth(left, above, upper_left)
            else:
                raise ValidationError(f"{path}: unsupported PNG filter {filter_type}")
            row[index] = (value + predictor) & 0xFF
        rgba[y * stride : (y + 1) * stride] = row
        previous = row
    return Image(width, height, bytes(rgba))


def count_pixels(image: Image, rectangle: tuple[int, int, int, int], kind: str) -> int:
    x0, y0, x1, y1 = rectangle

    def matches(red: int, green: int, blue: int) -> bool:
        if kind == "cyan":
            return blue - red >= 20 and green - red >= 18 and blue >= 90
        if kind == "white":
            return max(red, green, blue) - min(red, green, blue) <= 45 \
                and red + green + blue >= 300
        if kind == "yellow":
            return red >= 100 and green >= 70 and blue <= 110 and red - blue >= 45
        if kind == "map_route":
            return 60 <= red <= 100 and 150 <= green <= 205 and 170 <= blue <= 220
        raise AssertionError(f"unknown pixel class {kind}")

    count = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            red, green, blue, _ = image.pixel(x, y)
            if matches(red, green, blue):
                count += 1
    return count


HUD_RULES = (
    ("NORTHERN", (31, 31, 144, 49), "cyan", 550),
    ("VALLEY", (151, 31, 241, 49), "cyan", 320),
    ("EXPRESS", (247, 31, 352, 49), "cyan", 450),
    ("DESTINATION label", (31, 56, 117, 71), "white", 280),
    ("destination name", (129, 56, 325, 71), "white", 600),
    ("NEXT label", (31, 80, 78, 96), "yellow", 110),
    ("next station", (83, 80, 450, 96), "yellow", 350),
    ("speed", (31, 103, 90, 119), "white", 120),
    ("ETA", (110, 103, 210, 119), "white", 170),
)


def validate_hud(image: Image, label: str) -> None:
    failures = []
    for name, rectangle, kind, minimum in HUD_RULES:
        actual = count_pixels(image, rectangle, kind)
        if actual < minimum:
            failures.append(f"{name}: {actual} < {minimum}")
    if failures:
        raise ValidationError(f"{label}: incomplete HUD glyph coverage ({'; '.join(failures)})")


def validate_platform_shade(image: Image, label: str) -> None:
    """Check two unoccupied paving patches in the authored Stonebridge window view only."""
    def mean_brightness(y0: int) -> float:
        return sum(sum(image.pixel(x, y)[:3]) / 3
                   for y in range(y0, y0 + 25) for x in range(450, 750)) / 7500

    shade, sun = mean_brightness(520), mean_brightness(610)
    if shade < 20 or sun < 60 or shade > sun * 0.85:
        raise ValidationError(
            f"{label}: missing readable canopy shade (shade={shade:.1f}, sun={sun:.1f})"
        )


def validate_showcase(path: Path) -> Image:
    image = decode_png(path)
    validate_hud(image, str(path))
    if path.name == "stonebridge-platform.png":
        validate_platform_shade(image, str(path))
    if path.name == "route-map.png":
        route_pixels = count_pixels(image, (150, 275, 1130, 288), "map_route")
        if route_pixels < 1_000:
            raise ValidationError(
                f"{path}: route map line is incomplete ({route_pixels} < 1000 pixels)"
            )
    if path.name == "lakehurst-window.png":
        # This authored fixed daytime view must contain a visible water band below the skyline,
        # not merely report generated water triangles hidden behind an accidental terrain ridge.
        water_pixels = 0
        for y in range(360, 560):
            for x in range(0, 1280):
                red, green, blue, _ = image.pixel(x, y)
                water_pixels += blue > green + 5 and green > red + 15 and blue < 200
        if water_pixels < 5_000:
            raise ValidationError(f"{path}: lake view has too little visible water ({water_pixels})")
    return image


def validate_scene_materials(image: Image, label: str) -> None:
    """Reject a valid HUD capture whose main 3D view collapsed to opaque black.

    Sample a broad passenger-eye region at quarter resolution. This deliberately measures broad
    composition rather than exact pixels, so ordinary lighting, NPC, and material improvements do
    not turn the check into a brittle golden-image comparison.
    """
    black_pixels = 0
    sampled_pixels = 0
    quantized_colours: set[tuple[int, int, int]] = set()
    for y in range(130, 700, 4):
        for x in range(180, 1100, 4):
            red, green, blue, _ = image.pixel(x, y)
            sampled_pixels += 1
            if red + green + blue < 75:
                black_pixels += 1
            quantized_colours.add((red // 16, green // 16, blue // 16))

    black_fraction = black_pixels / sampled_pixels
    if black_fraction > 0.35:
        raise ValidationError(
            f"{label}: 3D scene is predominantly black "
            f"({black_fraction:.1%} > 35.0%)"
        )
    if len(quantized_colours) < 50:
        raise ValidationError(
            f"{label}: 3D scene has insufficient tonal diversity "
            f"({len(quantized_colours)} < 50 quantized colours)"
        )


def run_negative_self_test(image: Image) -> None:
    corrupted = bytearray(image.rgba)
    x0, y0, x1, y1 = (247, 31, 352, 49)
    for y in range(y0, y1):
        for x in range(x0, x1):
            offset = (y * image.width + x) * 4
            corrupted[offset : offset + 4] = bytes((24, 45, 59, 255))
    try:
        validate_hud(Image(image.width, image.height, bytes(corrupted)), "negative self-test")
    except ValidationError:
        return
    raise ValidationError("negative self-test accepted a capture with the EXPRESS glyphs removed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--quiet", action="store_true", help="only print validation failures")
    parser.add_argument(
        "--self-test", action="store_true",
        help="also prove that a deliberately removed HUD word is rejected",
    )
    parser.add_argument(
        "--scene-materials", action="store_true",
        help="also reject a predominantly black or tonally collapsed 3D scene",
    )
    parser.add_argument("screenshots", nargs="+", type=Path)
    arguments = parser.parse_args()

    try:
        images = []
        for path in arguments.screenshots:
            image = validate_showcase(path)
            if arguments.scene_materials:
                validate_scene_materials(image, str(path))
            images.append(image)
            if not arguments.quiet:
                print(f"Validated showcase capture: {path}")
        if arguments.self_test:
            run_negative_self_test(images[0])
            if not arguments.quiet:
                print("Validated negative incomplete-HUD self-test")
    except (OSError, ValidationError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
