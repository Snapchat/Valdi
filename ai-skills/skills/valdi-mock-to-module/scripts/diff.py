#!/usr/bin/env python3
"""
Generate a pixel diff between a design mock and a Valdi render snapshot,
including shadow detection, region masking, and per-segment breakdown.

Usage:
  python3 diff.py <mock_path> <render_path> [--output <output_path>]

  # Mask thumbnail regions (exclude photos from diff)
  python3 diff.py mock.png render.png --mask "10,5,35,35" "10,50,35,35"

  # Per-region breakdown (find which section of the screen needs work)
  python3 diff.py mock.png render.png --segments 4

Produces a side-by-side image: mock | render | amplified diff | shadow diff
- Panel 3 (amplified diff): bright pixels = color/content mismatches
- Panel 4 (shadow diff): blue = shadows present in mock but missing in render
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageChops, ImageEnhance, ImageFilter
except ImportError:
    print("Pillow not installed. Run: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)


def _crop_to_phone_frame(render: Image.Image) -> Image.Image:
    """
    Crop the desktop render to just the phone frame column.

    The PreviewRoot centers a white 390px phone frame on a #333 background.
    Scan the middle row for the white content region and crop to it.
    Falls back to the full image if no frame is detected.
    """
    render_rgb = render.convert("RGB")
    mid_y = render.height // 2
    row = [render_rgb.getpixel((x, mid_y)) for x in range(render.width)]

    # The phone frame background is white (#FFFFFF); the outer bg is #333.
    # Find the leftmost and rightmost bright columns.
    bg_threshold = 100  # below this = dark background
    left = None
    right = None
    for x, (r, g, b) in enumerate(row):
        if r > bg_threshold and g > bg_threshold and b > bg_threshold:
            if left is None:
                left = x
            right = x

    if left is not None and right is not None and (right - left) > 50:
        # Add small padding so we don't clip shadow edges
        pad = 4
        left = max(0, left - pad)
        right = min(render.width, right + pad)
        return render.crop((left, 0, right, render.height))

    # Fallback: no phone frame detected, return full image
    return render


def detect_missing_shadows(mock_rgb: Image.Image, render_rgb: Image.Image):
    """
    Detect shadow regions present in the mock but missing in the render.

    Shadows are soft, low-frequency luminance drops near element edges.
    Strategy:
      1. Convert both to grayscale.
      2. Heavy blur to isolate low-frequency luminance (shadow shapes).
      3. Detect edges in the un-blurred image (element boundaries).
      4. Dilate edges to create a "near-edge" zone where shadows live.
      5. In that zone, find where mock is notably darker than render
         (mock has shadow, render doesn't).
      6. Return a visualization image and count of shadow pixels.
    """
    mock_gray = mock_rgb.convert("L")
    render_gray = render_rgb.convert("L")

    # Step 1: Heavy blur to extract the low-frequency (shadow) component.
    # Shadows are gradual darkening, not sharp detail.
    blur_radius = 8
    mock_blurred = mock_gray.filter(ImageFilter.GaussianBlur(radius=blur_radius))
    render_blurred = render_gray.filter(ImageFilter.GaussianBlur(radius=blur_radius))

    # Step 2: Find element edges in the render (these are where shadows should appear).
    # Use a lighter blur + edge detection to find element boundaries.
    render_edges = render_gray.filter(ImageFilter.GaussianBlur(radius=1))
    render_edges = render_edges.filter(ImageFilter.FIND_EDGES)

    # Step 3: Dilate edges to create a "shadow zone" — the area near edges
    # where we'd expect drop shadows to appear (below/right of elements).
    # MaxFilter expands bright regions, effectively dilating the edge map.
    shadow_zone = render_edges.filter(ImageFilter.MaxFilter(size=11))
    # Also do the same from mock edges to catch elements that only exist in mock
    mock_edges = mock_gray.filter(ImageFilter.GaussianBlur(radius=1))
    mock_edges = mock_edges.filter(ImageFilter.FIND_EDGES)
    mock_zone = mock_edges.filter(ImageFilter.MaxFilter(size=11))

    # Combine both edge zones
    shadow_zone_bytes = list(shadow_zone.tobytes())
    mock_zone_bytes = list(mock_zone.tobytes())
    combined_zone = bytes(max(a, b) for a, b in zip(shadow_zone_bytes, mock_zone_bytes))
    shadow_zone = Image.frombytes("L", render_gray.size, combined_zone)

    # Step 4: Compare blurred luminance. Where mock is darker than render
    # near edges = shadow present in mock but missing in render.
    mock_blur_bytes = list(mock_blurred.tobytes())
    render_blur_bytes = list(render_blurred.tobytes())
    zone_bytes = list(shadow_zone.tobytes())

    # Thresholds — tuned to avoid false positives from content/color differences.
    # Only flag genuine shadow gradients: soft darkening near hard edges.
    darkness_threshold = 25   # mock must be at least this much darker than render
    edge_threshold = 30       # minimum edge strength to consider "near an edge"

    shadow_pixels = []
    missing_count = 0
    for i in range(len(mock_blur_bytes)):
        mock_val = mock_blur_bytes[i]
        render_val = render_blur_bytes[i]
        near_edge = zone_bytes[i] > edge_threshold

        # Mock is darker than render near an edge = missing shadow
        darkness_diff = render_val - mock_val  # positive = mock is darker
        if near_edge and darkness_diff > darkness_threshold:
            # Intensity proportional to how much shadow is missing
            intensity = min(255, darkness_diff * 3)
            shadow_pixels.append((0, intensity // 3, intensity))  # blue tint
            missing_count += 1
        else:
            shadow_pixels.append((0, 0, 0))

    # Build the shadow diff visualization
    shadow_img = Image.new("RGB", render_rgb.size)
    shadow_img.putdata(shadow_pixels)

    # Overlay faintly on the render so you can see WHERE shadows are missing
    render_dim = ImageEnhance.Brightness(render_rgb).enhance(0.4)
    overlay = ImageChops.add(render_dim, shadow_img)

    total_pixels = len(mock_blur_bytes)
    shadow_pct = round(100 * missing_count / total_pixels, 1) if total_pixels > 0 else 0

    return overlay, missing_count, shadow_pct


def _resolve_macos_path(path: str) -> str:
    """Handle macOS screenshot filenames with Unicode narrow no-break space (U+202F)."""
    if os.path.exists(path):
        return path
    # macOS uses U+202F (narrow no-break space) between time and AM/PM in screenshot filenames
    for marker in (' AM', ' PM'):
        fixed = path.replace(marker, '\u202f' + marker[1:])
        if os.path.exists(fixed):
            return fixed
    return path


def _auto_detect_masks(img: Image.Image) -> list[tuple[int, int, int, int]]:
    """
    Auto-detect thumbnail/avatar regions in the left column of a resized image.
    Scans for rectangular non-white blocks. Returns list of (x, y, w, h).
    """
    w, h = img.size
    img_rgb = img.convert("RGB")
    # Scan at ~11% from left (typical thumbnail center column)
    scan_x = int(w * 0.11)
    threshold = 230

    # Find vertical segments of non-white pixels
    segments = []
    in_thumb = False
    start_y = 0
    for y in range(h):
        r, g, b = img_rgb.getpixel((scan_x, y))
        is_content = r < threshold or g < threshold or b < threshold
        if is_content and not in_thumb:
            start_y = y
            in_thumb = True
        elif not is_content and in_thumb:
            seg_h = y - start_y
            if seg_h > 10:  # minimum thumbnail height in resized image
                segments.append((start_y, y))
            in_thumb = False
    if in_thumb and (h - start_y) > 10:
        segments.append((start_y, h))

    # Find horizontal extent for each segment
    results = []
    for y_start, y_end in segments:
        mid_y = (y_start + y_end) // 2
        left = None
        right = None
        for x in range(0, w // 3):
            r, g, b = img_rgb.getpixel((x, mid_y))
            if r < threshold or g < threshold or b < threshold:
                if left is None:
                    left = x
                right = x
        if left is not None and right is not None:
            thumb_w = right - left + 1
            thumb_h = y_end - y_start
            # Small padding around the detected region
            pad = 2
            results.append((max(0, left - pad), max(0, y_start - pad),
                            thumb_w + pad * 2, thumb_h + pad * 2))

    return results


def _apply_masks(img: Image.Image, mask_regions: list[tuple[int, int, int, int]], fill=(128, 128, 128)) -> Image.Image:
    """Black out specified rectangular regions. Coordinates are in the final (resized) image space."""
    masked = img.copy()
    for x, y, w, h in mask_regions:
        for py in range(max(0, y), min(y + h, masked.height)):
            for px in range(max(0, x), min(x + w, masked.width)):
                masked.putpixel((px, py), fill)
    return masked


def _segment_match(diff: Image.Image, num_segments: int = 4, threshold: int = 30) -> list[tuple[str, float]]:
    """
    Segment the diff image into horizontal bands and compute per-segment match %.
    Returns list of (label, match_pct) for each segment.
    """
    h = diff.height
    seg_h = h // num_segments
    results = []

    for i in range(num_segments):
        y_start = i * seg_h
        y_end = (i + 1) * seg_h if i < num_segments - 1 else h
        region = diff.crop((0, y_start, diff.width, y_end))
        region_bytes = list(region.convert("RGB").tobytes())
        pixels = [(region_bytes[j], region_bytes[j+1], region_bytes[j+2])
                   for j in range(0, len(region_bytes), 3)]
        matching = sum(1 for r, g, b in pixels if r < threshold and g < threshold and b < threshold)
        pct = round(100 * matching / len(pixels), 1) if pixels else 0
        # Label by vertical position
        frac_start = round(100 * y_start / h)
        frac_end = round(100 * y_end / h)
        label = f"rows {frac_start}-{frac_end}%"
        results.append((label, pct))

    return results


def generate_diff(mock_path: str, render_path: str, output_path: str, width: int = 400,
                  mask_regions: list[tuple[int, int, int, int]] | None = None,
                  auto_mask: bool = False,
                  num_segments: int = 0):
    mock = Image.open(_resolve_macos_path(mock_path))
    render = Image.open(_resolve_macos_path(render_path))

    # Crop mock: remove phone status bar and bottom border/nav.
    # 7% top and 5% bottom crops the phone chrome more aggressively,
    # producing better aspect ratio alignment with pure-content renders.
    status_bar = int(mock.height * 0.07)
    bottom_nav = int(mock.height * 0.05)
    mock_cropped = mock.crop((0, status_bar, mock.width, mock.height - bottom_nav))

    # Crop render to the phone frame column. The PreviewRoot uses a 390px-wide
    # phone frame centered in the window. Extract just that strip so the aspect
    # ratios match the phone mock instead of the desktop window.
    render_cropped = _crop_to_phone_frame(render)

    # Use the mock's aspect ratio as ground truth
    target_h = int(width * mock_cropped.height / mock_cropped.width)
    mock_resized = mock_cropped.resize((width, target_h), Image.LANCZOS).convert("RGB")
    render_resized = render_cropped.resize((width, target_h), Image.LANCZOS).convert("RGB")

    mock_rgb = mock_resized.copy()
    render_rgb = render_resized.copy()

    # Auto-detect thumbnail masks from the resized mock if requested
    if auto_mask:
        detected = _auto_detect_masks(mock_resized)
        if detected:
            if mask_regions is None:
                mask_regions = []
            mask_regions.extend(detected)
            print(f"Auto-detected {len(detected)} mask region(s): {detected}")

    # Apply masks to exclude known-image regions (e.g., photo thumbnails)
    mock_for_diff = mock_rgb
    render_for_diff = render_rgb
    if mask_regions:
        mock_for_diff = _apply_masks(mock_rgb, mask_regions)
        render_for_diff = _apply_masks(render_rgb, mask_regions)

    # Pixel diff, amplified 4x for visibility
    diff = ImageChops.difference(mock_for_diff, render_for_diff)
    diff_bright = ImageEnhance.Brightness(diff).enhance(4.0)

    # Shadow detection (on unmasked images — shadows matter everywhere)
    shadow_overlay, shadow_count, shadow_pct = detect_missing_shadows(mock_rgb, render_rgb)

    # Draw mask overlays on all panels so it's clear what's excluded
    if mask_regions:
        for panel_img in [mock_resized, render_resized, diff_bright]:
            for mx, my, mw, mh in mask_regions:
                for py in range(max(0, my), min(my + mh, panel_img.height)):
                    for px in range(max(0, mx), min(mx + mw, panel_img.width)):
                        # Semi-transparent red tint to mark masked areas
                        orig = panel_img.getpixel((px, py))
                        blended = (
                            min(255, orig[0] // 2 + 100),
                            orig[1] // 3,
                            orig[2] // 3,
                        )
                        panel_img.putpixel((px, py), blended)
                # Draw border around masked region
                for px in range(max(0, mx), min(mx + mw, panel_img.width)):
                    for edge_y in [my, my + mh - 1]:
                        if 0 <= edge_y < panel_img.height:
                            panel_img.putpixel((px, edge_y), (255, 0, 0))
                for py in range(max(0, my), min(my + mh, panel_img.height)):
                    for edge_x in [mx, mx + mw - 1]:
                        if 0 <= edge_x < panel_img.width:
                            panel_img.putpixel((edge_x, py), (255, 0, 0))

    # Side-by-side: mock | render | amplified diff | shadow diff
    gap = 6
    total_w = width * 4 + gap * 3
    canvas = Image.new("RGB", (total_w, target_h), (50, 50, 50))
    canvas.paste(mock_resized, (0, 0))
    canvas.paste(render_resized, (width + gap, 0))
    canvas.paste(diff_bright, (width * 2 + gap * 2, 0))
    canvas.paste(shadow_overlay, (width * 3 + gap * 3, 0))
    canvas.save(output_path)

    # Match percentage (% of pixels with diff < threshold)
    diff_bytes = list(diff.convert("RGB").tobytes())
    diff_pixels = [(diff_bytes[i], diff_bytes[i+1], diff_bytes[i+2]) for i in range(0, len(diff_bytes), 3)]
    threshold = 30  # per-channel tolerance
    matching = sum(1 for r, g, b in diff_pixels if r < threshold and g < threshold and b < threshold)
    total = len(diff_pixels)
    match_pct = round(100 * matching / total, 1)

    print(f"Diff saved: {output_path}")
    print(f"Match: {match_pct}% pixels within threshold ({threshold}/255 per channel)")
    if mask_regions:
        # Also compute unmasked match for reference
        raw_diff = ImageChops.difference(mock_rgb, render_rgb)
        raw_bytes = list(raw_diff.convert("RGB").tobytes())
        raw_pixels = [(raw_bytes[j], raw_bytes[j+1], raw_bytes[j+2]) for j in range(0, len(raw_bytes), 3)]
        raw_matching = sum(1 for r, g, b in raw_pixels if r < threshold and g < threshold and b < threshold)
        raw_pct = round(100 * raw_matching / len(raw_pixels), 1)
        print(f"Match (unmasked): {raw_pct}% — masked {len(mask_regions)} region(s)")
    print(f"Shadows: {shadow_pct}% of pixels have missing shadows ({shadow_count} pixels)")

    # Per-region segmented diff
    if num_segments > 0:
        segments = _segment_match(diff, num_segments, threshold)
        print(f"\nPer-region breakdown ({num_segments} segments):")
        for label, pct in segments:
            bar = '█' * int(pct / 5) + '░' * (20 - int(pct / 5))
            print(f"  {label:15s}  {bar} {pct}%")

    print(f"Mock: {mock.size} -> {mock_resized.size}, Render: {render.size} -> {render_resized.size}")
    return match_pct


def _parse_mask_region(spec: str) -> tuple[int, int, int, int]:
    """Parse 'x,y,w,h' into a tuple."""
    parts = spec.split(',')
    if len(parts) != 4:
        raise ValueError(f"Mask region must be x,y,w,h — got: {spec}")
    return tuple(int(p) for p in parts)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pixel diff: mock vs Valdi render")
    parser.add_argument("mock", help="Path to the design mock image")
    parser.add_argument("render", help="Path to the rendered snapshot")
    parser.add_argument("--output", "-o", default="/tmp/mock-vs-render-diff.png", help="Output diff image path")
    parser.add_argument("--width", "-w", type=int, default=400, help="Target comparison width in pixels")
    parser.add_argument("--mask", nargs="*", default=[],
                        help='Regions to exclude from diff: "x,y,w,h" (in resized image coords)')
    parser.add_argument("--auto-mask", action="store_true",
                        help="Auto-detect thumbnail/avatar regions in the mock and mask them")
    parser.add_argument("--segments", "-s", type=int, default=0,
                        help="Number of horizontal segments for per-region breakdown (0=disabled)")
    args = parser.parse_args()

    masks = [_parse_mask_region(m) for m in args.mask] if args.mask else None

    generate_diff(args.mock, args.render, args.output, args.width,
                  mask_regions=masks, auto_mask=args.auto_mask, num_segments=args.segments)
