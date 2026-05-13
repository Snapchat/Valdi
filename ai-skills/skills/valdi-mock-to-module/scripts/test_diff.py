#!/usr/bin/env python3
"""Tests for diff.py — pixel diff and shadow detection logic."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from diff import (
    _crop_to_phone_frame,
    _auto_detect_masks,
    _apply_masks,
    _segment_match,
    _parse_mask_region,
    detect_missing_shadows,
    generate_diff,
)

try:
    from PIL import Image, ImageChops
except ImportError:
    print("Pillow required for tests: pip3 install Pillow", file=sys.stderr)
    sys.exit(1)


class TestCropToPhoneFrame(unittest.TestCase):
    def test_crops_centered_white_frame(self):
        # Simulate a desktop window with dark bg and white phone frame
        img = Image.new('RGB', (800, 600), (51, 51, 51))  # #333 background
        # White phone frame 390px wide, centered
        left = (800 - 390) // 2
        for y in range(600):
            for x in range(left, left + 390):
                img.putpixel((x, y), (255, 255, 255))
        cropped = _crop_to_phone_frame(img)
        # Should be approximately 390px wide (with small padding)
        self.assertGreater(cropped.width, 380)
        self.assertLess(cropped.width, 410)

    def test_returns_full_image_when_no_frame(self):
        # All-dark image — no phone frame to detect
        img = Image.new('RGB', (400, 600), (51, 51, 51))
        cropped = _crop_to_phone_frame(img)
        self.assertEqual(cropped.size, img.size)

    def test_returns_full_image_when_all_white(self):
        img = Image.new('RGB', (400, 600), (255, 255, 255))
        cropped = _crop_to_phone_frame(img)
        # Entire image is "bright", so it returns the whole thing
        self.assertEqual(cropped.width, 400)


class TestAutoDetectMasks(unittest.TestCase):
    def test_detects_dark_blocks(self):
        img = Image.new('RGB', (200, 200), (255, 255, 255))
        # Dark block in left column
        for y in range(40, 80):
            for x in range(15, 35):
                img.putpixel((x, y), (50, 50, 50))
        masks = _auto_detect_masks(img)
        self.assertGreater(len(masks), 0)

    def test_no_masks_on_white(self):
        img = Image.new('RGB', (200, 200), (255, 255, 255))
        masks = _auto_detect_masks(img)
        self.assertEqual(len(masks), 0)


class TestApplyMasks(unittest.TestCase):
    def test_fills_mask_regions(self):
        img = Image.new('RGB', (100, 100), (255, 0, 0))
        masked = _apply_masks(img, [(10, 10, 20, 20)], fill=(128, 128, 128))
        # Center of masked region should be gray
        self.assertEqual(masked.getpixel((20, 20)), (128, 128, 128))
        # Outside mask should still be red
        self.assertEqual(masked.getpixel((0, 0)), (255, 0, 0))

    def test_clamps_to_image_bounds(self):
        img = Image.new('RGB', (50, 50), (0, 255, 0))
        # Mask extends beyond image — should not crash
        masked = _apply_masks(img, [(40, 40, 20, 20)])
        self.assertEqual(masked.size, (50, 50))

    def test_empty_masks_returns_copy(self):
        img = Image.new('RGB', (50, 50), (100, 100, 100))
        masked = _apply_masks(img, [])
        self.assertEqual(masked.getpixel((25, 25)), (100, 100, 100))


class TestSegmentMatch(unittest.TestCase):
    def test_identical_images_100_percent(self):
        # All-black diff = perfect match
        diff = Image.new('RGB', (100, 100), (0, 0, 0))
        segments = _segment_match(diff, num_segments=4, threshold=30)
        self.assertEqual(len(segments), 4)
        for label, pct in segments:
            self.assertEqual(pct, 100.0)

    def test_all_different_0_percent(self):
        # All-white diff = no match
        diff = Image.new('RGB', (100, 100), (255, 255, 255))
        segments = _segment_match(diff, num_segments=2, threshold=30)
        self.assertEqual(len(segments), 2)
        for label, pct in segments:
            self.assertEqual(pct, 0.0)

    def test_mixed_segments(self):
        # Top half black (match), bottom half white (no match)
        diff = Image.new('RGB', (100, 100), (0, 0, 0))
        for y in range(50, 100):
            for x in range(100):
                diff.putpixel((x, y), (255, 255, 255))
        segments = _segment_match(diff, num_segments=2, threshold=30)
        self.assertEqual(len(segments), 2)
        self.assertEqual(segments[0][1], 100.0)  # top = match
        self.assertEqual(segments[1][1], 0.0)    # bottom = no match

    def test_label_format(self):
        diff = Image.new('RGB', (100, 100), (0, 0, 0))
        segments = _segment_match(diff, num_segments=4)
        labels = [s[0] for s in segments]
        self.assertIn('rows 0-25%', labels)
        self.assertIn('rows 75-100%', labels)


class TestParseMaskRegion(unittest.TestCase):
    def test_valid_region(self):
        self.assertEqual(_parse_mask_region('10,20,30,40'), (10, 20, 30, 40))

    def test_invalid_region(self):
        with self.assertRaises(ValueError):
            _parse_mask_region('10,20,30')
        with self.assertRaises(ValueError):
            _parse_mask_region('10,20,30,40,50')


class TestDetectMissingShadows(unittest.TestCase):
    def test_identical_images_no_shadows(self):
        img = Image.new('RGB', (100, 100), (200, 200, 200))
        overlay, count, pct = detect_missing_shadows(img, img)
        self.assertEqual(count, 0)
        self.assertEqual(pct, 0)

    def test_returns_image_of_correct_size(self):
        mock = Image.new('RGB', (100, 100), (180, 180, 180))
        render = Image.new('RGB', (100, 100), (220, 220, 220))
        overlay, count, pct = detect_missing_shadows(mock, render)
        self.assertEqual(overlay.size, (100, 100))


class TestGenerateDiff(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmpdir)

    def test_identical_images_high_match(self):
        img = Image.new('RGB', (400, 800), (255, 255, 255))
        mock_path = os.path.join(self.tmpdir, 'mock.png')
        render_path = os.path.join(self.tmpdir, 'render.png')
        output_path = os.path.join(self.tmpdir, 'diff.png')
        img.save(mock_path)
        img.save(render_path)
        match_pct = generate_diff(mock_path, render_path, output_path)
        self.assertGreater(match_pct, 90.0)
        self.assertTrue(os.path.exists(output_path))

    def test_output_is_4_panels_wide(self):
        img = Image.new('RGB', (400, 800), (200, 200, 200))
        mock_path = os.path.join(self.tmpdir, 'mock.png')
        render_path = os.path.join(self.tmpdir, 'render.png')
        output_path = os.path.join(self.tmpdir, 'diff.png')
        img.save(mock_path)
        img.save(render_path)
        generate_diff(mock_path, render_path, output_path, width=200)
        result = Image.open(output_path)
        # 4 panels of 200px + 3 gaps of 6px = 818
        self.assertEqual(result.width, 200 * 4 + 6 * 3)

    def test_with_segments(self):
        img = Image.new('RGB', (400, 800), (255, 255, 255))
        mock_path = os.path.join(self.tmpdir, 'mock.png')
        render_path = os.path.join(self.tmpdir, 'render.png')
        output_path = os.path.join(self.tmpdir, 'diff.png')
        img.save(mock_path)
        img.save(render_path)
        # Should not crash with segments enabled
        match_pct = generate_diff(mock_path, render_path, output_path, num_segments=4)
        self.assertGreater(match_pct, 90.0)

    def test_with_mask_regions(self):
        img = Image.new('RGB', (400, 800), (255, 255, 255))
        mock_path = os.path.join(self.tmpdir, 'mock.png')
        render_path = os.path.join(self.tmpdir, 'render.png')
        output_path = os.path.join(self.tmpdir, 'diff.png')
        img.save(mock_path)
        img.save(render_path)
        match_pct = generate_diff(mock_path, render_path, output_path,
                                  mask_regions=[(10, 10, 50, 50)])
        self.assertGreater(match_pct, 90.0)


if __name__ == '__main__':
    unittest.main()
