#!/usr/bin/env python3
"""
Extract dominant colors from regions of a mock image using k-means clustering.

Usage:
  # Auto-detect thumbnail regions in a list UI
  python3 extract_colors.py <mock_path> --auto-thumbnails

  # Extract from specific bounding boxes (x,y,w,h)
  python3 extract_colors.py <mock_path> --regions "dodgers:45,50,70,70" "larams:45,145,70,70"

  # Extract UI element colors by clicking coordinates
  python3 extract_colors.py <mock_path> --sample "live_badge:230,135" "green_dot:90,360"
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow not installed. Run: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)


def _resolve_macos_path(path: str) -> str:
    """Handle macOS screenshot filenames with Unicode narrow no-break space (U+202F)."""
    if os.path.exists(path):
        return path
    for marker in (' AM', ' PM'):
        fixed = path.replace(marker, '\u202f' + marker[1:])
        if os.path.exists(fixed):
            return fixed
    return path


def dominant_color_kmeans(pixels: list[tuple[int, int, int]], k: int = 3) -> str:
    """
    Find the dominant color using a simple k-means implementation.
    Returns the hex color of the largest cluster, preferring saturated colors.
    No numpy/scipy dependency.
    """
    if not pixels:
        return '#000000'

    if len(pixels) < k:
        r = sum(p[0] for p in pixels) // len(pixels)
        g = sum(p[1] for p in pixels) // len(pixels)
        b = sum(p[2] for p in pixels) // len(pixels)
        return f'#{r:02X}{g:02X}{b:02X}'

    # Initialize centroids by spreading across the pixel range
    import random
    random.seed(42)
    centroids = random.sample(pixels, k)

    for _ in range(20):  # iterations
        clusters: list[list[tuple[int, int, int]]] = [[] for _ in range(k)]

        for p in pixels:
            dists = [sum((p[c] - centroids[i][c]) ** 2 for c in range(3)) for i in range(k)]
            clusters[dists.index(min(dists))].append(p)

        new_centroids = []
        for cluster in clusters:
            if cluster:
                r = sum(p[0] for p in cluster) // len(cluster)
                g = sum(p[1] for p in cluster) // len(cluster)
                b = sum(p[2] for p in cluster) // len(cluster)
                new_centroids.append((r, g, b))
            else:
                new_centroids.append(random.choice(pixels))

        if new_centroids == centroids:
            break
        centroids = new_centroids

    # Score each cluster: prefer larger + more saturated
    best = None
    best_score = -1
    for i, cluster in enumerate(clusters):
        if not cluster:
            continue
        r, g, b = centroids[i]
        # Saturation = how far from gray
        mean_val = (r + g + b) / 3
        saturation = (abs(r - mean_val) + abs(g - mean_val) + abs(b - mean_val)) / 3
        # Score = size * (1 + saturation bonus)
        score = len(cluster) * (1 + saturation / 128)
        # Penalty for near-white and near-black (these are usually background)
        if mean_val > 240 or mean_val < 15:
            score *= 0.1
        if score > best_score:
            best_score = score
            best = centroids[i]

    if best is None:
        best = centroids[0]
    return f'#{best[0]:02X}{best[1]:02X}{best[2]:02X}'


def sample_point(img: Image.Image, x: int, y: int, radius: int = 3) -> str:
    """Sample a small area around a point and return the median color."""
    colors = []
    w, h = img.size
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            px = max(0, min(x + dx, w - 1))
            py = max(0, min(y + dy, h - 1))
            colors.append(img.getpixel((px, py)))
    colors.sort(key=lambda c: c[0] + c[1] + c[2])
    mid = colors[len(colors) // 2]
    return f'#{mid[0]:02X}{mid[1]:02X}{mid[2]:02X}'


def extract_region(img: Image.Image, x: int, y: int, w: int, h: int) -> str:
    """Extract dominant color from a rectangular region."""
    pixels = []
    img_w, img_h = img.size
    for py in range(max(0, y), min(y + h, img_h)):
        for px in range(max(0, x), min(x + w, img_w)):
            pixels.append(img.getpixel((px, py)))
    return dominant_color_kmeans(pixels)


def auto_detect_thumbnails(img: Image.Image) -> list[tuple[str, int, int, int, int]]:
    """
    Auto-detect thumbnail regions in a list UI.
    Scans for rectangular non-white blocks in the left column.
    Returns list of (name, x, y, w, h).
    """
    w, h = img.size
    # Scan vertical center of typical thumbnail column (x=50-90 range)
    scan_x = int(w * 0.11)  # ~11% from left

    # Find vertical segments of non-white pixels
    segments = []
    in_thumb = False
    start_y = 0
    threshold = 230

    for y in range(0, h):
        r, g, b = img.getpixel((scan_x, y))
        is_content = r < threshold or g < threshold or b < threshold

        if is_content and not in_thumb:
            start_y = y
            in_thumb = True
        elif not is_content and in_thumb:
            seg_h = y - start_y
            if seg_h > 20:  # minimum thumbnail height
                segments.append((start_y, y))
            in_thumb = False

    if in_thumb:
        segments.append((start_y, h))

    # Now find horizontal extent for each segment
    results = []
    for idx, (y_start, y_end) in enumerate(segments):
        mid_y = (y_start + y_end) // 2
        # Scan horizontally to find left and right edges
        left = None
        right = None
        for x in range(0, w // 3):
            r, g, b = img.getpixel((x, mid_y))
            if r < threshold or g < threshold or b < threshold:
                if left is None:
                    left = x
                right = x

        if left is not None and right is not None:
            thumb_w = right - left + 1
            thumb_h = y_end - y_start
            results.append((f'thumb_{idx}', left, y_start, thumb_w, thumb_h))

    return results


def auto_detect_colored_regions(img: Image.Image) -> list[tuple[str, int, int, int, int]]:
    """
    Auto-detect ALL saturated/colored regions in the image (buttons, badges, icons).
    Scans the entire image for clusters of non-white, non-black, saturated pixels.
    Returns list of (name, x, y, w, h).
    """
    w, h = img.size
    threshold_sat = 25  # minimum saturation to be "colored"
    step = 3

    # Find all saturated pixels
    colored_pixels = []
    for y in range(0, h, step):
        for x in range(0, w, step):
            r, g, b = img.getpixel((x, y))
            mean = (r + g + b) / 3
            sat = max(abs(r - mean), abs(g - mean), abs(b - mean))
            if sat > threshold_sat and 20 < mean < 245:
                colored_pixels.append((x, y))

    if not colored_pixels:
        return []

    # Simple flood-fill clustering: group nearby pixels
    clusters: list[list[tuple[int, int]]] = []
    used = set()
    for px, py in colored_pixels:
        if (px, py) in used:
            continue
        # BFS to find connected colored pixels
        cluster = [(px, py)]
        queue = [(px, py)]
        used.add((px, py))
        while queue:
            cx, cy = queue.pop(0)
            for nx, ny in colored_pixels:
                if (nx, ny) not in used and abs(nx - cx) <= step * 2 and abs(ny - cy) <= step * 2:
                    used.add((nx, ny))
                    cluster.append((nx, ny))
                    queue.append((nx, ny))
        if len(cluster) >= 3:  # minimum cluster size
            clusters.append(cluster)

    results = []
    for idx, cluster in enumerate(clusters):
        xs = [p[0] for p in cluster]
        ys = [p[1] for p in cluster]
        x_min, x_max = min(xs), max(xs)
        y_min, y_max = min(ys), max(ys)
        cw = x_max - x_min + step
        ch = y_max - y_min + step
        results.append((f'region_{idx}', x_min, y_min, cw, ch))

    return results


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Extract colors from a mock image')
    parser.add_argument('mock', help='Path to mock image')
    parser.add_argument('--auto-thumbnails', action='store_true',
                        help='Auto-detect thumbnail regions')
    parser.add_argument('--auto-colors', action='store_true',
                        help='Auto-detect ALL colored regions (buttons, badges, icons)')
    parser.add_argument('--regions', nargs='*', default=[],
                        help='Named regions: "name:x,y,w,h"')
    parser.add_argument('--sample', nargs='*', default=[],
                        help='Sample points: "name:x,y"')
    args = parser.parse_args()

    img = Image.open(_resolve_macos_path(args.mock)).convert('RGB')
    print(f'Image: {img.size[0]}x{img.size[1]}')

    if args.auto_thumbnails:
        print('\n--- Auto-detected thumbnails ---')
        thumbs = auto_detect_thumbnails(img)
        for name, x, y, tw, th in thumbs:
            color = extract_region(img, x, y, tw, th)
            print(f"  {name:25s}  {color}  ({x},{y},{tw}x{th})")

    if args.auto_colors:
        print('\n--- Auto-detected colored regions ---')
        regions = auto_detect_colored_regions(img)
        for name, x, y, rw, rh in regions:
            color = extract_region(img, x, y, rw, rh)
            print(f"  {name:25s}  {color}  ({x},{y},{rw}x{rh})")

    for region_spec in args.regions:
        name, coords = region_spec.split(':')
        x, y, rw, rh = map(int, coords.split(','))
        color = extract_region(img, x, y, rw, rh)
        print(f"  {name:25s}  {color}  ({x},{y},{rw}x{rh})")

    for sample_spec in args.sample:
        name, coords = sample_spec.split(':')
        x, y = map(int, coords.split(','))
        color = sample_point(img, x, y)
        print(f"  {name:25s}  {color}  ({x},{y})")
