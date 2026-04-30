#!/usr/bin/env python3
"""Tests for extract_colors.py — color extraction logic."""

import os
import sys
import tempfile
import unittest

# Add the script directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract_colors import (
    dominant_color_kmeans,
    sample_point,
    extract_region,
    auto_detect_thumbnails,
    auto_detect_colored_regions,
    _resolve_macos_path,
)

try:
    from PIL import Image
except ImportError:
    print("Pillow required for tests: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)


class TestDominantColorKmeans(unittest.TestCase):
    def test_empty_pixels(self):
        self.assertEqual(dominant_color_kmeans([]), '#000000')

    def test_single_pixel(self):
        result = dominant_color_kmeans([(255, 0, 0)])
        self.assertEqual(result, '#FF0000')

    def test_two_pixels(self):
        # With < k pixels, averages them
        result = dominant_color_kmeans([(255, 0, 0), (255, 0, 0)])
        self.assertEqual(result, '#FF0000')

    def test_uniform_color(self):
        pixels = [(100, 50, 200)] * 100
        result = dominant_color_kmeans(pixels)
        self.assertEqual(result, '#6432C8')

    def test_prefers_saturated_over_white(self):
        # Mix of white (background) and red (content)
        pixels = [(255, 255, 255)] * 80 + [(200, 30, 30)] * 20
        result = dominant_color_kmeans(pixels)
        # Should pick the red cluster, not white (which gets penalized)
        r = int(result[1:3], 16)
        g = int(result[3:5], 16)
        b = int(result[5:7], 16)
        self.assertGreater(r, g)  # Red-ish
        self.assertGreater(r, b)

    def test_prefers_larger_cluster(self):
        # Large blue cluster vs small green cluster
        pixels = [(30, 30, 200)] * 90 + [(30, 200, 30)] * 10
        result = dominant_color_kmeans(pixels)
        b = int(result[5:7], 16)
        g = int(result[3:5], 16)
        self.assertGreater(b, g)  # Should be blue-ish

    def test_penalizes_near_black(self):
        pixels = [(5, 5, 5)] * 80 + [(100, 50, 200)] * 20
        result = dominant_color_kmeans(pixels)
        # The purple should win despite fewer pixels
        r = int(result[1:3], 16)
        self.assertGreater(r, 20)  # Not near-black


class TestSamplePoint(unittest.TestCase):
    def test_center_pixel_of_solid_image(self):
        img = Image.new('RGB', (20, 20), (128, 64, 32))
        result = sample_point(img, 10, 10)
        self.assertEqual(result, '#804020')

    def test_edge_pixel_clamped(self):
        img = Image.new('RGB', (10, 10), (255, 0, 0))
        # Sampling at (0, 0) with radius=3 should clamp and not crash
        result = sample_point(img, 0, 0, radius=3)
        self.assertEqual(result, '#FF0000')

    def test_sample_returns_median(self):
        # Create an image with a gradient-like pattern
        img = Image.new('RGB', (20, 20), (100, 100, 100))
        # Put a bright pixel in the center
        img.putpixel((10, 10), (255, 255, 255))
        result = sample_point(img, 10, 10, radius=3)
        # Median of mostly gray + one white should still be gray
        r = int(result[1:3], 16)
        self.assertLess(r, 200)  # Not the outlier white


class TestExtractRegion(unittest.TestCase):
    def test_solid_region(self):
        img = Image.new('RGB', (100, 100), (0, 128, 255))
        result = extract_region(img, 10, 10, 20, 20)
        self.assertEqual(result, '#0080FF')

    def test_region_clamps_to_image_bounds(self):
        img = Image.new('RGB', (50, 50), (255, 0, 0))
        # Region extends beyond image — should not crash
        result = extract_region(img, 40, 40, 20, 20)
        self.assertEqual(result, '#FF0000')


class TestAutoDetectThumbnails(unittest.TestCase):
    def test_detects_dark_block_in_left_column(self):
        # White image with a dark square in the left column
        img = Image.new('RGB', (400, 400), (255, 255, 255))
        # Draw a dark block at x=30-70, y=50-100 (in the left 11% scan zone)
        for y in range(50, 100):
            for x in range(30, 70):
                img.putpixel((x, y), (50, 50, 50))
        results = auto_detect_thumbnails(img)
        self.assertGreater(len(results), 0)
        name, x, y, w, h = results[0]
        self.assertGreaterEqual(y, 40)
        self.assertLessEqual(y + h, 110)

    def test_no_thumbnails_on_white_image(self):
        img = Image.new('RGB', (400, 400), (255, 255, 255))
        results = auto_detect_thumbnails(img)
        self.assertEqual(len(results), 0)


class TestAutoDetectColoredRegions(unittest.TestCase):
    def test_detects_saturated_region(self):
        # White image with a small bright blue rectangle
        img = Image.new('RGB', (200, 200), (255, 255, 255))
        for y in range(50, 70):
            for x in range(100, 140):
                img.putpixel((x, y), (0, 100, 255))
        results = auto_detect_colored_regions(img)
        self.assertGreater(len(results), 0)

    def test_no_colored_regions_on_white_image(self):
        img = Image.new('RGB', (200, 200), (255, 255, 255))
        results = auto_detect_colored_regions(img)
        self.assertEqual(len(results), 0)

    def test_ignores_near_white_and_near_black(self):
        # Image with only near-white and near-black — no saturated pixels
        img = Image.new('RGB', (200, 200), (250, 250, 250))
        for y in range(50, 100):
            for x in range(50, 100):
                img.putpixel((x, y), (10, 10, 10))
        results = auto_detect_colored_regions(img)
        self.assertEqual(len(results), 0)


class TestResolveMacosPath(unittest.TestCase):
    def test_existing_path_unchanged(self):
        with tempfile.NamedTemporaryFile(delete=False) as f:
            result = _resolve_macos_path(f.name)
            self.assertEqual(result, f.name)
        os.unlink(f.name)

    def test_nonexistent_path_returned_as_is(self):
        path = '/tmp/nonexistent_test_file_12345.png'
        result = _resolve_macos_path(path)
        self.assertEqual(result, path)


if __name__ == '__main__':
    unittest.main()
