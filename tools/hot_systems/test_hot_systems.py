import os
import re
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import hot_systems as hs  # noqa: E402


def _systems(yaml_text: str) -> list:
    return hs.parse_systems(hs.parse_yaml_subset(yaml_text), "<test>")


MINIMAL = """
version: 1
systems:
  - id: teardown
    label: Runtime teardown
    gate: flag
    note: Lifetime.
    globs:
      - valdi/src/valdi/runtime/JavaScript/**
      - valdi/src/valdi/runtime/{Runtime,RuntimeManager}.*
  - id: build
    label: Build
    gate: guard
    note: Build rules.
    globs:
      - "**/*.bzl"
      - .bazelrc
"""


class GlobTest(unittest.TestCase):
    def assertMatches(self, glob, path):
        self.assertTrue(hs.compile_glob(glob).match(path), f"{glob} should match {path}")

    def assertNoMatch(self, glob, path):
        self.assertFalse(hs.compile_glob(glob).match(path), f"{glob} should not match {path}")

    def test_double_star_spans_directories(self):
        self.assertMatches("snap_drawing/**", "snap_drawing/src/a/b.cpp")
        self.assertMatches("**/*.bzl", "valdi.bzl")
        self.assertMatches("**/*.bzl", "bzl/valdi/rules.bzl")
        self.assertMatches("a/**/b.txt", "a/b.txt")
        self.assertMatches("a/**/b.txt", "a/x/y/b.txt")
        self.assertNoMatch("snap_drawing/**", "snap_drawing_extra/a.cpp")

    def test_single_star_stays_in_segment(self):
        self.assertMatches("valdi/src/valdi/ios/Views/SCValdiText*", "valdi/src/valdi/ios/Views/SCValdiTextView.m")
        self.assertNoMatch("valdi/*.cpp", "valdi/src/a.cpp")
        self.assertMatches("a/?.c", "a/b.c")
        self.assertNoMatch("a/?.c", "a/bb.c")

    def test_braces(self):
        glob = "valdi/src/valdi/runtime/{Runtime,RuntimeManager}.*"
        self.assertMatches(glob, "valdi/src/valdi/runtime/Runtime.cpp")
        self.assertMatches(glob, "valdi/src/valdi/runtime/RuntimeManager.hpp")
        self.assertNoMatch(glob, "valdi/src/valdi/runtime/RuntimeX.cpp")
        self.assertMatches("x/{a.kt,b/**}", "x/b/c/d.kt")
        self.assertEqual(hs._expand_braces("{a,{b,c}}d"), ["ad", "bd", "cd"])
        with self.assertRaises(ValueError):
            hs.compile_glob("a/{b,c")

    def test_literal_characters_are_escaped(self):
        self.assertMatches("UIImage+Valdi.*", "UIImage+Valdi.h")
        self.assertNoMatch(".bazelrc", "xbazelrc")


class YamlSubsetTest(unittest.TestCase):
    def test_parses_nested_blocks(self):
        data = hs.parse_yaml_subset(MINIMAL)
        self.assertEqual(data["version"], 1)
        self.assertEqual(data["systems"][1]["globs"], ["**/*.bzl", ".bazelrc"])
        self.assertEqual(data["systems"][0]["globs"][1], "valdi/src/valdi/runtime/{Runtime,RuntimeManager}.*")

    def test_comments_and_quotes(self):
        data = hs.parse_yaml_subset("a: 'it''s' # trailing\nb: \"x # y\"\nc: d#e\n")
        self.assertEqual(data, {"a": "it's", "b": "x # y", "c": "d#e"})

    def test_rejects_unsupported_syntax(self):
        for text in ("a: [1, 2]\n", "a: {b: c}\n", "a: &x b\n", "a: |\n  b\n", "a:\n\tb: c\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                hs.parse_yaml_subset(text)

    def test_rejects_bad_structure(self):
        for text in ("a: b\na: c\n", "a: b\n   c: d\n", "- a\nb: c\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                hs.parse_yaml_subset(text)

    def test_matches_pyyaml_on_real_file(self):
        try:
            import yaml  # type: ignore[import-untyped]
        except ImportError:
            self.skipTest("PyYAML not installed")
        with open(hs.DEFAULT_CONFIG, encoding="utf-8") as f:
            text = f.read()
        self.assertEqual(hs.parse_yaml_subset(text), yaml.safe_load(text))


class SchemaTest(unittest.TestCase):
    def test_valid(self):
        systems = _systems(MINIMAL)
        self.assertEqual([s.id for s in systems], ["teardown", "build"])
        self.assertEqual(systems[1].gate, "guard")

    def test_prefix(self):
        systems = hs.parse_systems(hs.parse_yaml_subset(MINIMAL), "<test>", prefix="client/src/open_source/")
        self.assertEqual(systems[1].globs[1], "client/src/open_source/.bazelrc")

    def test_errors(self):
        bad = {
            "version": MINIMAL.replace("version: 1", "version: 2"),
            "gate": MINIMAL.replace("gate: guard", "gate: maybe"),
            "id": MINIMAL.replace("id: build", "id: Build"),
            "duplicate": MINIMAL.replace("id: build", "id: teardown"),
            "absolute": MINIMAL.replace("- .bazelrc", "- /etc/passwd"),
            "label": MINIMAL.replace("    label: Build\n", ""),
        }
        for name, text in bad.items():
            with self.subTest(name), self.assertRaises(ValueError):
                _systems(text)


CATCH_ALL = MINIMAL + """  - id: framework
    label: Other framework code
    gate: flag
    catch_all: true
    note: Everything else.
    globs:
      - valdi/src/**
    exclude:
      - "**/test/**"
      - "**/*.md"
"""


class CatchAllTest(unittest.TestCase):
    def test_only_claims_files_no_named_system_matched(self):
        systems = _systems(CATCH_ALL)
        self.assertTrue(systems[-1].catch_all)
        matches = hs.match_files(
            systems,
            [
                "valdi/src/valdi/runtime/Runtime.cpp",
                "valdi/src/valdi/runtime/Utils/Other.cpp",
                "valdi/src/valdi/test/Foo.cpp",
                "valdi/src/README.md",
            ],
        )
        by_id = {m.system.id: m.files for m in matches}
        self.assertEqual(by_id["teardown"], ["valdi/src/valdi/runtime/Runtime.cpp"])
        self.assertEqual(by_id["framework"], ["valdi/src/valdi/runtime/Utils/Other.cpp"])

    def test_exclude_requires_catch_all_and_one_catch_all(self):
        with self.assertRaises(ValueError):
            _systems(MINIMAL.replace("      - .bazelrc\n", "      - .bazelrc\n    exclude:\n      - x\n"))
        with self.assertRaises(ValueError):
            _systems(CATCH_ALL + "  - id: other\n    label: O\n    gate: flag\n    catch_all: true\n    note: n\n    globs:\n      - x/**\n")

    def test_bools(self):
        self.assertEqual(hs.parse_yaml_subset("a: true\nb: false\nc: 'true'\n"), {"a": True, "b": False, "c": "true"})

    def test_overlay_keeps_catch_all(self):
        merged = hs.parse_overlay(
            hs.parse_yaml_subset("version: 1\nsystems:\n  - id: framework\n    globs:\n      - client/x/**\n"),
            "<overlay>",
            _systems(CATCH_ALL),
        )
        framework = merged[-1]
        self.assertTrue(framework.catch_all)
        self.assertIn("client/x/**", framework.globs)
        self.assertIsNone(framework.matching_glob("valdi/src/a/test/b.cpp"))


class OverlayTest(unittest.TestCase):
    OVERLAY = """
version: 1
systems:
  - id: teardown
    globs:
      - client/src/composer_snap_modules/**
  - id: coreui
    label: coreui components
    gate: flag
    note: Shared components.
    globs:
      - client/src/composer_modules/src/composer/coreui/**
"""

    def test_merge(self):
        public = hs.parse_systems(hs.parse_yaml_subset(MINIMAL), "<test>", prefix="client/src/open_source/")
        merged = hs.parse_overlay(hs.parse_yaml_subset(self.OVERLAY), "<overlay>", public)
        self.assertEqual([s.id for s in merged], ["teardown", "build", "coreui"])
        self.assertIn("client/src/composer_snap_modules/**", merged[0].globs)
        self.assertEqual(merged[0].label, "Runtime teardown")
        self.assertNotIn("client/src/composer_snap_modules/**", public[0].globs)

    def test_overlay_cannot_relabel_public_system(self):
        text = self.OVERLAY.replace("  - id: teardown\n", "  - id: teardown\n    gate: guard\n")
        with self.assertRaises(ValueError):
            hs.parse_overlay(hs.parse_yaml_subset(text), "<overlay>", _systems(MINIMAL))


class MatchAndRenderTest(unittest.TestCase):
    def test_match_and_labels(self):
        systems = _systems(MINIMAL)
        matches = hs.match_files(
            systems,
            [
                "valdi/src/valdi/runtime/Runtime.cpp",
                "valdi/src/valdi/runtime/JavaScript/JavaScriptRuntime.cpp",
                "README.md",
                "",
            ],
        )
        self.assertEqual([m.system.id for m in matches], ["teardown"])
        self.assertEqual(len(matches[0].files), 2)
        self.assertEqual(hs.labels(matches), ["hot/teardown"])

    def test_render(self):
        matches = hs.match_files(_systems(MINIMAL), ["valdi/src/valdi/runtime/Runtime.cpp", "x/y.bzl"])
        body = hs.render_comment(matches, "https://policy", "https://flags")
        self.assertTrue(body.startswith(hs.COMMENT_MARKER))
        self.assertIn("**Runtime teardown** (`hot/teardown`, gate: flag)", body)
        self.assertIn("`valdi/src/valdi/runtime/Runtime.cpp`", body)
        self.assertIn("**guard** systems", body)
        self.assertIn("- [ ] Not significant", body)
        self.assertIn("never blocks a merge", body)

    def test_render_strips_prefix_and_caps_files(self):
        files = [f"client/src/open_source/valdi/src/valdi/runtime/JavaScript/f{i}.cpp" for i in range(8)]
        matches = hs.match_files(
            hs.parse_systems(hs.parse_yaml_subset(MINIMAL), "<test>", prefix="client/src/open_source/"), files
        )
        body = hs.render_comment(matches, "p", "f", strip_prefix="client/src/open_source/")
        self.assertIn("`valdi/src/valdi/runtime/JavaScript/f0.cpp`", body)
        self.assertIn("and 3 more", body)

    def test_render_guide_link(self):
        matches = hs.match_files(_systems(MINIMAL), ["valdi/src/valdi/runtime/Runtime.cpp"])
        self.assertNotIn("What to do", hs.render_comment(matches, "p", "f"))
        body = hs.render_comment(matches, "p", "f", guide_link="https://guide")
        self.assertIn("**What to do:** [your PR got this comment, now what?](https://guide)", body)

    def test_render_empty(self):
        self.assertEqual(hs.render_comment([], "p", "f"), "")


class RealFileTest(unittest.TestCase):
    """The checked-in list: valid, public-safe, and every glob still matches a file."""

    @classmethod
    def setUpClass(cls):
        cls.systems = hs.load_systems()
        with open(hs.DEFAULT_CONFIG, encoding="utf-8") as f:
            cls.text = f.read()

    def test_every_glob_matches_a_file(self):
        files = hs.tracked_files(hs.OPEN_SOURCE_ROOT)
        self.assertGreater(len(files), 1000)
        self.assertEqual(hs.dead_globs(self.systems, files), [])

    def test_no_internal_references(self):
        # The file ships to the public repo. Internal paths, tickets and hosts stay in
        # the internal overlay.
        forbidden = re.compile(r"client/src/|composer|sc[-]corp|jira|\bCOF\b|\bVDD\b|deliver|[A-Z]{2,}-\d+", re.I)
        self.assertEqual(forbidden.findall(self.text), [])

    def test_ids_are_unique_area_ids(self):
        ids = [s.id for s in self.systems]
        self.assertEqual(len(ids), len(set(ids)))
        for system in self.systems:
            self.assertIn(system.gate, hs.GATES)


if __name__ == "__main__":
    unittest.main()
