#!/usr/bin/env python3
"""Tests for find_icon.py — fuzzy icon matching logic."""

import os
import sys
import tempfile
import unittest

# Add the script directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from find_icon import split_camel_case, expand_query, score_icon, parse_icons, find_icons


class TestSplitCamelCase(unittest.TestCase):
    def test_simple_camel_case(self):
        self.assertEqual(split_camel_case('chatBubbleFill'), ['chat', 'bubble', 'fill'])

    def test_multi_word(self):
        self.assertEqual(split_camel_case('arrowRightShareFill'), ['arrow', 'right', 'share', 'fill'])

    def test_single_word(self):
        self.assertEqual(split_camel_case('camera'), ['camera'])

    def test_consecutive_capitals(self):
        # 'SC' treated as one segment
        parts = split_camel_case('SCMyModule')
        self.assertEqual(parts[0], 'sc')

    def test_fill_suffix(self):
        parts = split_camel_case('gearFill')
        self.assertEqual(parts, ['gear', 'fill'])

    def test_xmark(self):
        parts = split_camel_case('xmarkFill')
        self.assertEqual(parts, ['xmark', 'fill'])


class TestExpandQuery(unittest.TestCase):
    def test_known_synonym(self):
        result = expand_query(['settings'])
        self.assertIn('gear', result)
        self.assertIn('settings', result)

    def test_no_synonym(self):
        result = expand_query(['chat'])
        self.assertEqual(result, ['chat'])

    def test_multiple_synonyms(self):
        result = expand_query(['search'])
        self.assertIn('magnifying', result)
        self.assertIn('glass', result)

    def test_mixed_known_unknown(self):
        result = expand_query(['send', 'big'])
        self.assertIn('send', result)
        self.assertIn('big', result)
        self.assertIn('arrow', result)


class TestScoreIcon(unittest.TestCase):
    def test_exact_match_scores_high(self):
        icon_words = ['chat', 'bubble', 'fill']
        query_words = ['chat', 'bubble']
        expanded = expand_query(query_words)
        score = score_icon(icon_words, query_words, expanded)
        self.assertGreater(score, 0)

    def test_no_match_scores_zero(self):
        icon_words = ['gear', 'fill']
        query_words = ['chat', 'bubble']
        expanded = expand_query(query_words)
        score = score_icon(icon_words, query_words, expanded)
        self.assertEqual(score, 0.0)

    def test_synonym_match(self):
        icon_words = ['gear', 'fill']
        query_words = ['settings']
        expanded = expand_query(query_words)
        score = score_icon(icon_words, query_words, expanded)
        self.assertGreater(score, 0)

    def test_fill_variant_boosted(self):
        fill_words = ['chat', 'bubble', 'fill']
        stroke_words = ['chat', 'bubble', 'stroke']
        query_words = ['chat', 'bubble']
        expanded = expand_query(query_words)
        fill_score = score_icon(fill_words, query_words, expanded)
        stroke_score = score_icon(stroke_words, query_words, expanded)
        self.assertGreater(fill_score, stroke_score)

    def test_empty_query_scores_zero(self):
        icon_words = ['chat', 'bubble']
        score = score_icon(icon_words, [], [])
        self.assertEqual(score, 0.0)


class TestParseIcons(unittest.TestCase):
    def test_parses_static_getters(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.tsx', delete=False) as f:
            f.write("""
export class SIGIcon {
    static get chatBubbleFill(): Asset { return Asset.fromModule('sig_chat_bubble_fill'); }
    static get gearFill(): Asset { return Asset.fromModule('sig_gear_fill'); }
    static get bellStroke(): Asset { return Asset.fromModule('sig_bell_stroke'); }
}
""")
            f.flush()
            icons = parse_icons(f.name)
        os.unlink(f.name)
        self.assertEqual(icons, ['chatBubbleFill', 'gearFill', 'bellStroke'])

    def test_empty_file(self):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.tsx', delete=False) as f:
            f.write("// empty\n")
            f.flush()
            icons = parse_icons(f.name)
        os.unlink(f.name)
        self.assertEqual(icons, [])


class TestFindIcons(unittest.TestCase):
    def setUp(self):
        self.icons_file = tempfile.NamedTemporaryFile(mode='w', suffix='.tsx', delete=False)
        self.icons_file.write("""
export class SIGIcon {
    static get chatBubbleFill(): Asset { return Asset.fromModule('a'); }
    static get chatBubbleStroke(): Asset { return Asset.fromModule('b'); }
    static get gearFill(): Asset { return Asset.fromModule('c'); }
    static get gearStroke(): Asset { return Asset.fromModule('d'); }
    static get bellFill(): Asset { return Asset.fromModule('e'); }
    static get arrowRightFill(): Asset { return Asset.fromModule('f'); }
    static get mapPinFill(): Asset { return Asset.fromModule('g'); }
    static get personFill(): Asset { return Asset.fromModule('h'); }
    static get magnifyingGlassFill(): Asset { return Asset.fromModule('i'); }
    static get threeDotHorizontalFill(): Asset { return Asset.fromModule('j'); }
}
""")
        self.icons_file.flush()

    def tearDown(self):
        os.unlink(self.icons_file.name)

    def test_direct_match(self):
        results = find_icons('chat bubble', self.icons_file.name)
        self.assertTrue(len(results) > 0)
        # chatBubbleFill should be the top result
        self.assertEqual(results[0][0], 'chatBubbleFill')

    def test_synonym_match(self):
        results = find_icons('settings', self.icons_file.name)
        self.assertTrue(len(results) > 0)
        top_names = [r[0] for r in results]
        self.assertTrue(any('gear' in name.lower() for name in top_names))

    def test_notification_synonym(self):
        results = find_icons('notification', self.icons_file.name)
        self.assertTrue(len(results) > 0)
        top_names = [r[0] for r in results]
        self.assertIn('bellFill', top_names)

    def test_search_synonym(self):
        results = find_icons('search', self.icons_file.name)
        self.assertTrue(len(results) > 0)
        self.assertEqual(results[0][0], 'magnifyingGlassFill')

    def test_no_match(self):
        results = find_icons('xyznonexistent', self.icons_file.name)
        self.assertEqual(len(results), 0)

    def test_top_n_limiting(self):
        results = find_icons('fill', self.icons_file.name, top_n=3)
        self.assertTrue(len(results) <= 3)

    def test_profile_synonym(self):
        results = find_icons('profile', self.icons_file.name)
        self.assertTrue(len(results) > 0)
        top_names = [r[0] for r in results]
        self.assertIn('personFill', top_names)

    def test_menu_synonym(self):
        results = find_icons('menu', self.icons_file.name)
        self.assertTrue(len(results) > 0)
        top_names = [r[0] for r in results]
        self.assertIn('threeDotHorizontalFill', top_names)


if __name__ == '__main__':
    unittest.main()
