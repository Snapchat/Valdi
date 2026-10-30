#!/usr/bin/env python3
"""Unit tests for check_native_impl.py. Stdlib only: `python3 -m unittest` from this directory."""

import contextlib
import io
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import check_native_impl as cni  # noqa: E402

TESTDATA = HERE / "testdata"


def _fixture(name: str) -> Path:
    # Fixtures carry a .fixture suffix so linters and build globs never treat them as real sources.
    return TESTDATA / f"{name}.fixture"


def _read(name: str) -> str:
    return _fixture(name).read_text(encoding="utf-8")


class DeclarationTests(unittest.TestCase):
    def setUp(self):
        self.declared = cni.parse_declarations(_read("NativeTemplateElements.d.ts"))

    def test_resolves_extends_chain(self):
        # Inherited from ViewAttributes / LayoutAttributes / ContainerTemplateElement.
        for name in ("onTap", "backgroundColor", "maskImage", "margin", "marginTop", "children"):
            self.assertIn(name, self.declared)
        self.assertEqual(self.declared["maskImage"], ["view"])
        self.assertEqual(self.declared["margin"], ["layout", "view"])

    def test_own_members_and_multiline_extends(self):
        self.assertIn("font", self.declared)
        self.assertIn("iosOnlyThing", self.declared)
        self.assertIn("ref", self.declared)

    def test_skips_slots_and_unannotated_interfaces(self):
        self.assertNotIn("slotOnlyName", self.declared)
        self.assertNotIn("unrelated", self.declared)

    def test_ignores_nested_object_type_keys(self):
        self.assertIn("shadow", self.declared)
        self.assertNotIn("color", self.declared)
        self.assertNotIn("radius", self.declared)

    def test_annotation_args(self):
        elements = cni.parse_elements(_read("NativeTemplateElements.d.ts"))
        self.assertEqual(elements["View"]["ios"], "SCValdiView")
        self.assertEqual(elements["View"]["jsx"], "view")
        self.assertEqual(elements["Layout"]["layout"], "true")
        self.assertEqual(elements["Slot"]["slot"], "true")


class ExtractorTests(unittest.TestCase):
    def test_ios(self):
        names = cni.extract_ios(_read("UIView+Fixture.m"))
        self.assertEqual(
            names,
            {"onTap", "backgroundColor", "maskImage", "shadowComposite", "shadow", "font", "iosOnlyThing"},
        )

    def test_ios_ignores_commented_out_registration(self):
        self.assertNotIn("commentedOut", cni.extract_ios(_read("UIView+Fixture.m")))

    def test_ios_constant_from_another_file(self):
        text = '[[SCNValdiCoreCompositeAttributePart alloc] initWithAttribute:kLineHeightAttribute type:0];'
        self.assertEqual(cni.extract_ios(text), set())
        self.assertEqual(cni.extract_ios(text, {"kLineHeightAttribute": "lineHeight"}), {"lineHeight"})

    def test_ios_edge_prefix(self):
        text = (
            'for (NSString *direction in @[ @"Top", @"Right", @"Bottom", @"Left" ]) {\n'
            '  [parts addObject:[[SCNValdiCoreCompositeAttributePart alloc]\n'
            '    initWithAttribute:[@"touchAreaExtension" stringByAppendingString:direction] type:0]];\n'
            "}"
        )
        self.assertEqual(
            cni.extract_ios(text),
            {
                "touchAreaExtension",
                "touchAreaExtensionTop",
                "touchAreaExtensionRight",
                "touchAreaExtensionBottom",
                "touchAreaExtensionLeft",
            },
        )

    def test_android(self):
        names = cni.extract_android(_read("ViewAttributesBinder.kt"))
        self.assertEqual(names, {"onTap", "backgroundColor", "maskImage", "shadowComposite", "shadow", "font"})

    def test_android_multiline_call(self):
        self.assertEqual(cni.extract_android('ctx.bindBooleanAttribute(\n    "enabled",\n    false)'), {"enabled"})

    def test_shared_cpp(self):
        names = cni.extract_shared_cpp(_read("YogaAttributes.cpp"))
        self.assertEqual(names, {"margin", "marginTop", "marginRight", "marginBottom", "marginLeft"})

    def test_shared_cpp_patterns(self):
        text = (
            'binder.bindViewNodeCallback("onViewCreate", &ViewNode::setCb);\n'
            'parts.emplace_back(STRING_LITERAL("transform"), AttributeType::String, true, false);\n'
            'auto srcAttributeId = _attributeIds.getIdForName("src");\n'
        )
        self.assertEqual(cni.extract_shared_cpp(text), {"onViewCreate", "transform", "src"})

    def test_shared_cpp_ignores_preprocessor_lookups(self):
        # How DefaultAttributeProcessors.cpp references maskImage: a preprocessor, not a binder.
        text = (
            'auto maskImageAttributeId = attributesManager.getAttributeIds().getIdForName("maskImage");\n'
            "attributesManager.registerPreprocessor(maskImageAttributeId, &preprocessGradient);\n"
        )
        self.assertEqual(cni.extract_shared_cpp(text), set())


class AllowlistTests(unittest.TestCase):
    def test_parse(self):
        entries = cni.parse_allowlist(
            "# comment\n\nfoo ios Only on iOS.\nbar ios,android Web only.\nkey runtime Renderer.\n"
        )
        self.assertEqual(entries["foo"], ({"ios"}, "Only on iOS."))
        self.assertEqual(entries["bar"][0], {"ios", "android"})
        self.assertEqual(entries["key"][0], {"runtime"})

    def test_requires_reason(self):
        with self.assertRaises(ValueError):
            cni.parse_allowlist("foo ios\n")

    def test_rejects_unknown_platform(self):
        with self.assertRaises(ValueError):
            cni.parse_allowlist("foo windows Because.\n")

    def test_rejects_runtime_combined(self):
        with self.assertRaises(ValueError):
            cni.parse_allowlist("foo runtime,ios Because.\n")

    def test_rejects_duplicates(self):
        with self.assertRaises(ValueError):
            cni.parse_allowlist("foo ios A.\nfoo android B.\n")

    def test_checked_in_allowlist_parses(self):
        cni.parse_allowlist(cni.DEFAULT_ALLOWLIST.read_text(encoding="utf-8"))


class EvaluateTests(unittest.TestCase):
    DECLARED = {"maskImage": ["view"], "onTap": ["view"], "onTapPredicate": ["view"], "key": ["view"]}

    def _implemented(self, ios=(), android=()):
        return {"ios": set(ios), "android": set(android)}

    def test_all_implemented(self):
        impl = self._implemented(ios={"maskImage", "onTap"}, android={"maskImage", "onTap"})
        errors, stale = cni.evaluate(self.DECLARED, impl, set(), {"key": ({"runtime"}, "r")})
        self.assertEqual((errors, stale), ([], []))

    def test_missing_ios(self):
        impl = self._implemented(ios={"onTap"}, android={"maskImage", "onTap"})
        errors, _ = cni.evaluate(self.DECLARED, impl, set(), {"key": ({"runtime"}, "r")})
        self.assertEqual(len(errors), 1)
        self.assertIn("maskImage", errors[0])
        self.assertIn("no ios implementation", errors[0])

    def test_shared_counts_for_both(self):
        impl = self._implemented(ios={"onTap"}, android={"onTap"})
        errors, _ = cni.evaluate(self.DECLARED, impl, {"maskImage"}, {"key": ({"runtime"}, "r")})
        self.assertEqual(errors, [])

    def test_derived_predicate_requires_base(self):
        impl = self._implemented(ios={"maskImage"}, android={"maskImage", "onTap"})
        errors, _ = cni.evaluate(self.DECLARED, impl, set(), {"key": ({"runtime"}, "r")})
        self.assertEqual(sorted(e.split(" ")[0] for e in errors), ["onTap", "onTapPredicate"])

    def test_allowlisted_platform(self):
        impl = self._implemented(ios={"onTap"}, android={"maskImage", "onTap"})
        allow = {"key": ({"runtime"}, "r"), "maskImage": ({"ios"}, "reason")}
        self.assertEqual(cni.evaluate(self.DECLARED, impl, set(), allow), ([], []))

    def test_allowlist_for_one_platform_does_not_cover_the_other(self):
        impl = self._implemented(ios={"onTap"}, android={"onTap"})
        allow = {"key": ({"runtime"}, "r"), "maskImage": ({"ios"}, "reason")}
        errors, _ = cni.evaluate(self.DECLARED, impl, set(), allow)
        self.assertEqual(len(errors), 1)
        self.assertIn("no android implementation", errors[0])

    def test_stale_when_now_implemented(self):
        impl = self._implemented(ios={"maskImage", "onTap"}, android={"maskImage", "onTap"})
        allow = {"key": ({"runtime"}, "r"), "maskImage": ({"ios"}, "reason")}
        _, stale = cni.evaluate(self.DECLARED, impl, set(), allow)
        self.assertEqual(len(stale), 1)
        self.assertIn("now implemented", stale[0])

    def test_stale_when_no_longer_declared(self):
        impl = self._implemented(ios={"maskImage", "onTap"}, android={"maskImage", "onTap"})
        allow = {"key": ({"runtime"}, "r"), "gone": ({"runtime"}, "reason")}
        _, stale = cni.evaluate(self.DECLARED, impl, set(), allow)
        self.assertEqual(stale, ["gone: allowlisted but no longer declared in TypeScript"])


class EndToEndTests(unittest.TestCase):
    """Lay the fixtures out at the real source paths and run the whole check."""

    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root)
        layout = {
            "NativeTemplateElements.d.ts": cni.DECLARATIONS_FILE,
            "UIView+Fixture.m": "valdi/src/valdi/ios/Categories/UIView+Fixture.m",
            "ViewAttributesBinder.kt": "valdi/src/java/com/snap/valdi/attributes/impl/ViewAttributesBinder.kt",
            "YogaAttributes.cpp": "valdi/src/valdi/runtime/Attributes/Yoga/YogaAttributes.cpp",
        }
        for src, dest in layout.items():
            target = self.root / dest
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(_fixture(src), target)
        self.allowlist = self.root / "allowlist.txt"
        self.allowlist.write_text(
            "children runtime JSX children.\nref runtime Renderer.\niosOnlyThing android iOS only.\n",
            encoding="utf-8",
        )

    def _run(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cni.run(self.root, self.allowlist)
        return code, out.getvalue()

    def test_passes(self):
        code, out = self._run()
        self.assertEqual(code, 0, out)

    def test_fails_when_ios_mask_image_registration_is_deleted(self):
        path = self.root / "valdi/src/valdi/ios/Categories/UIView+Fixture.m"
        text = path.read_text(encoding="utf-8")
        start = text.index('    [attributesBinder bindAttribute:@"maskImage"')
        end = text.index("resetBlock:nil];", start) + len("resetBlock:nil];")
        path.write_text(text[:start] + text[end:], encoding="utf-8")

        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("maskImage (on <view>): no ios implementation", out)

    def test_fails_when_extractors_match_nothing(self):
        (self.root / "valdi/src/java/com/snap/valdi/attributes/impl/ViewAttributesBinder.kt").write_text(
            "// empty\n", encoding="utf-8"
        )
        code, out = self._run()
        self.assertEqual(code, 1)
        self.assertIn("found no Android attribute registrations", out)


if __name__ == "__main__":
    unittest.main()
