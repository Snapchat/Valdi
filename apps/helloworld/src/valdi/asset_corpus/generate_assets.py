#!/usr/bin/env python3
"""Regenerates the synthetic images in res/ (next to this script, wherever it is run from).

Standard library only, so every PNG variant (bit depth, color type, interlacing) is written byte
for byte here rather than depending on whatever an image library chooses to emit.

    python3 apps/helloworld/src/valdi/asset_corpus/generate_assets.py
"""

import os
import struct
import zlib

RES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "res")

COLOR_GRAY = 0
COLOR_RGB = 2
COLOR_PALETTE = 3
COLOR_GRAY_ALPHA = 4
COLOR_RGBA = 6

CHANNELS = {COLOR_GRAY: 1, COLOR_RGB: 3, COLOR_PALETTE: 1, COLOR_GRAY_ALPHA: 2, COLOR_RGBA: 4}

# (x start, y start, x step, y step) for each Adam7 pass.
ADAM7 = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]


def _chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def _pack_row(samples, bit_depth):
    if bit_depth == 8:
        return bytes(samples)
    if bit_depth == 16:
        return b"".join(struct.pack(">H", s) for s in samples)
    per_byte = 8 // bit_depth
    out = bytearray()
    for i in range(0, len(samples), per_byte):
        byte = 0
        group = samples[i:i + per_byte]
        for j, s in enumerate(group):
            byte |= s << (8 - bit_depth * (j + 1))
        out.append(byte)
    return bytes(out)


def _filter_sub(row, bpp):
    out = bytearray(row)
    for i in range(len(row) - 1, bpp - 1, -1):
        out[i] = (row[i] - row[i - bpp]) & 0xFF
    return b"\x01" + bytes(out)


def _scanlines(pixel, width, height, color_type, bit_depth, xs, ys):
    bpp = max(1, CHANNELS[color_type] * bit_depth // 8)
    out = bytearray()
    for y in ys:
        samples = []
        for x in xs:
            samples.extend(pixel(x, y))
        out += _filter_sub(_pack_row(samples, bit_depth), bpp)
    return bytes(out)


def write_png(name, width, height, color_type, bit_depth, pixel, palette=None, trns=None, interlace=False,
              extra_chunks=()):
    """pixel(x, y) returns the tuple of samples for one pixel, already scaled to bit_depth."""
    raw = bytearray()
    if interlace:
        for x0, y0, dx, dy in ADAM7:
            xs = range(x0, width, dx)
            ys = range(y0, height, dy)
            if len(xs) and len(ys):
                raw += _scanlines(pixel, width, height, color_type, bit_depth, xs, ys)
    else:
        raw += _scanlines(pixel, width, height, color_type, bit_depth, range(width), range(height))

    data = b"\x89PNG\r\n\x1a\n"
    data += _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 1 if interlace else 0))
    for kind, payload in extra_chunks:
        data += _chunk(kind, payload)
    if palette is not None:
        data += _chunk(b"PLTE", b"".join(bytes(c) for c in palette))
    if trns is not None:
        data += _chunk(b"tRNS", bytes(trns))
    data += _chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += _chunk(b"IEND", b"")

    with open(os.path.join(RES_DIR, name), "wb") as f:
        f.write(data)


def _ramp(v, size, maxval=255):
    return v * maxval // max(1, size - 1)


def generate_pngs():
    s = 48  # @3x of a 16pt image.

    write_png("png_rgba_alpha@3x.png", s, s, COLOR_RGBA, 8,
              lambda x, y: (_ramp(x, s), 64, _ramp(y, s), _ramp(x + y, 2 * s - 1)))

    write_png("png_gray_alpha@3x.png", s, s, COLOR_GRAY_ALPHA, 8,
              lambda x, y: (_ramp(x, s), 255 - _ramp(y, s)))

    palette8 = [(i, 255 - i, (i * 7) & 0xFF) for i in range(0, 256, 4)]
    write_png("png_palette_trns@3x.png", s, s, COLOR_PALETTE, 8,
              lambda x, y: ((x + y) % len(palette8),),
              palette=palette8, trns=[_ramp(i, len(palette8)) for i in range(len(palette8))])

    palette4 = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0), (0, 255, 255), (255, 0, 255), (0, 0, 0),
                (255, 255, 255)]
    write_png("png_palette_4bit@3x.png", s, s, COLOR_PALETTE, 4,
              lambda x, y: (((x // 6) + (y // 6)) % len(palette4),), palette=palette4)

    write_png("png_rgb16@3x.png", s, s, COLOR_RGB, 16,
              lambda x, y: (_ramp(x, s, 65535), _ramp(y, s, 65535), 65535 - _ramp(x, s, 65535)))

    write_png("png_rgba16@3x.png", s, s, COLOR_RGBA, 16,
              lambda x, y: (_ramp(x, s, 65535), 30000, _ramp(y, s, 65535), 65535 - _ramp(y, s, 65535)))

    write_png("png_interlaced@3x.png", s, s, COLOR_RGBA, 8,
              lambda x, y: (255 if (x // 8 + y // 8) % 2 else 0, _ramp(x, s), _ramp(y, s), 128 + _ramp(x, s, 127)),
              interlace=True)

    write_png("png_gamma_srgb@3x.png", s, s, COLOR_RGB, 8,
              lambda x, y: (_ramp(x, s), _ramp(y, s), 128),
              extra_chunks=[(b"gAMA", struct.pack(">I", 45455)), (b"sRGB", b"\x00")])

    big = 3072  # @3x of a 1024pt image: 9.4M pixels, the size of a full-screen background.
    write_png("png_large@3x.png", big, big, COLOR_RGB, 8,
              lambda x, y: (x * 255 // (big - 1), y * 255 // (big - 1), 96))

    # Nine-patch: a 1px border where black pixels mark the stretchable area (top, left) and the
    # content padding (bottom, right). The interior is a rounded-looking button body.
    n = 30
    body = (40, 120, 220, 255)

    def nine_patch(x, y):
        edge = x in (0, n - 1) or y in (0, n - 1)
        if edge:
            if (y == 0 and 12 <= x <= 17) or (x == 0 and 12 <= y <= 17):
                return (0, 0, 0, 255)
            if (y == n - 1 and 5 <= x <= n - 6) or (x == n - 1 and 5 <= y <= n - 6):
                return (0, 0, 0, 255)
            return (0, 0, 0, 0)
        inner_corner = min(x - 1, n - 2 - x) + min(y - 1, n - 2 - y)
        return body if inner_corner >= 3 else (0, 0, 0, 0)

    write_png("nine_patch.9.png", n, n, COLOR_RGBA, 8, nine_patch)


def main():
    os.makedirs(RES_DIR, exist_ok=True)
    generate_pngs()


if __name__ == "__main__":
    main()
