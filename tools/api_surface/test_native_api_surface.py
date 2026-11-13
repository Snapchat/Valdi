#!/usr/bin/env python3
"""Tests for the native API surface check. Stdlib only.

Run from open_source/: python3 -m unittest discover -s tools/api_surface -p 'test_*.py'

The end-to-end tests copy the scanned sources into a temp directory, break
them, and run check_api_surface.py against the committed baselines.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
OPEN_SOURCE = HERE.parent.parent
sys.path.insert(0, str(HERE))

import extract_native_api as ena  # noqa: E402

CHECK = HERE / "check_api_surface.py"
TS_SOURCES = [
    "src/valdi_modules/src/valdi/valdi_tsx/src/NativeTemplateElements.d.ts",
    "src/valdi_modules/src/valdi/valdi_core/src/Component.ts",
]
ANSI = re.compile(r"\x1b\[[0-9;]*m")


def _parse(parser, text, name="Test.kt"):
    col = ena._Collector()
    parser(text, name, col)
    return ena._finish(col)


class KotlinParserTests(unittest.TestCase):
    SRC = """
package com.example

import android.view.View

open class Widget(context: Context) : View(context) {
    var label: String? = null
    protected open fun onLabel(value: String) {}
    private fun hidden() {}
    internal fun alsoHidden() {}
    override fun onLayout(changed: Boolean, l: Int, t: Int, r: Int, b: Int) {}

    fun bind(ctx: AttributesBindingContext<Widget>) {
        ctx.bindStringAttribute("label", true, { v, s, _ -> }, { v, _ -> })
        val local = object : Runnable { override fun run() { fun nested() {} } }
    }

    companion object {
        const val TAG = "Widget"
    }

    enum class Mode(val raw: Int) {
        A(0), B(1);
        fun next(): Mode = this
    }
}

private class Secret {
    fun leak() {}
}

fun Widget.extension(): Int = 1
"""

    def setUp(self):
        self.api = _parse(ena._parse_kotlin, self.SRC, "Widget.kt")

    def test_public_and_protected_members(self):
        members = self.api["com.example.Widget"]["members"]
        self.assertEqual(members["label"], ["var label: String?"])
        self.assertEqual(members["onLabel"], ["protected open fun onLabel(value: String)"])
        self.assertIn("bind", members)

    def test_skips_private_internal_override_and_locals(self):
        members = self.api["com.example.Widget"]["members"]
        for name in ("hidden", "alsoHidden", "onLayout", "local", "run", "nested"):
            self.assertNotIn(name, members)
        self.assertNotIn("com.example.Secret", self.api)

    def test_declaration_and_nested_types(self):
        self.assertEqual(
            self.api["com.example.Widget"]["declaration"],
            "open class Widget(context: Context): View(context)",
        )
        self.assertIn("TAG", self.api["com.example.Widget.Companion"]["members"])
        mode = self.api["com.example.Widget.Mode"]["members"]
        self.assertEqual(sorted(k for k in mode if k in ("A", "B")), ["A", "B"])
        self.assertIn("next", mode)

    def test_multiline_primary_constructor_params_are_not_members(self):
        api = _parse(
            ena._parse_kotlin,
            "package com.example\n\nclass Options(\n    val isDebug: Boolean,\n    var level: Int\n) {\n"
            "    fun describe(): String = \"\"\n}\n",
            "Options.kt",
        )
        self.assertNotIn("com.example.OptionsKt", api)
        self.assertEqual(sorted(api["com.example.Options"]["members"]), ["describe"])
        self.assertEqual(
            api["com.example.Options"]["declaration"], "class Options(val isDebug: Boolean, var level: Int)"
        )

    def test_supertype_after_comment_or_blank_line(self):
        api = _parse(
            ena._parse_kotlin,
            "package com.example\n\nclass Spaced(context: Context)\n    // why\n\n    : View(context) {\n"
            "    fun visible() {}\n}\n",
            "Spaced.kt",
        )
        self.assertEqual(api["com.example.Spaced"]["declaration"], "class Spaced(context: Context): View(context)")
        self.assertIn("visible", api["com.example.Spaced"]["members"])

    def test_bindings_and_top_level(self):
        self.assertEqual(self.api["com.example.Widget"]["bindings"], ["label"])
        self.assertIn("extension", self.api["com.example.WidgetKt"]["members"])


class JavaParserTests(unittest.TestCase):
    def test_public_members_only(self):
        api = _parse(
            ena._parse_java,
            """
package com.example;
public class Action {
    public static final String NAME = "x";
    private int hidden;
    public Action(int a) { if (a > 0) { hidden = a; } }
    protected void perform(Object[] args) {}
    void packagePrivate() {}
    public interface Listener { void onAction(); }
}
""",
            "Action.java",
        )
        members = api["com.example.Action"]["members"]
        self.assertEqual(sorted(members), ["Action", "NAME", "perform"])
        self.assertIn("onAction", api["com.example.Action.Listener"]["members"])


class ObjCParserTests(unittest.TestCase):
    def test_interfaces_protocols_categories(self):
        api = _parse(
            ena._parse_objc_header,
            """
#import <UIKit/UIKit.h>
@protocol Forward;
typedef NS_ENUM(NSInteger, SCMode) { SCModeA = 0, SCModeB };
extern CGFloat SCHelper(CGFloat value);
@interface SCView : UIView <SCThing>
@property (nonatomic, copy) void (^onTap)(void);
@property (nonatomic, readonly) BOOL ready NS_SWIFT_NAME(isReady);
- (instancetype)initWithFrame:(CGRect)frame
                      context:(id)context;
+ (void)bindAttributes:(id)binder;
@end
@interface UIView (Valdi)
- (void)valdi_reset;
@end
@interface SCView ()
- (void)privateThing;
@end
@protocol SCThing <NSObject>
- (void)required;
@optional
- (void)maybe:(int)value;
@end
""",
            "SCView.h",
        )
        view = api["SCView"]["members"]
        self.assertEqual(
            sorted(view), ["+bindAttributes:", "-initWithFrame:context:", "onTap", "ready"]
        )
        self.assertEqual(api["SCView"]["declaration"], "@interface SCView: UIView <SCThing>")
        self.assertIn("-valdi_reset", api["UIView (Valdi)"]["members"])
        self.assertEqual(
            api["@protocol SCThing"]["members"]["-maybe:"], ["@optional - (void)maybe: (int)value"]
        )
        self.assertIn("SCModeB", api["enum SCMode"]["members"])
        self.assertIn("SCHelper", api["<extern>"]["members"])
        self.assertNotIn("@protocol Forward", api)

    def test_property_with_nested_macro_arguments(self):
        api = _parse(
            ena._parse_objc_header,
            "@interface SCField : UIView\n"
            "@property (nonatomic, strong) UITextSelectionInteraction *selectionInteraction API_AVAILABLE(ios(13.0));\n"
            "@property (nonatomic) BOOL ready NS_SWIFT_NAME(isReady) API_AVAILABLE(ios(13.0)) NS_UNAVAILABLE;\n"
            "@end\n",
            "SCField.h",
        )
        self.assertEqual(sorted(api["SCField"]["members"]), ["ready", "selectionInteraction"])

    def test_bindings_attach_to_public_classes(self):
        col = ena._Collector()
        ena._parse_objc_header("@interface SCView : UIView\n@end\n", "SCView.h", col)
        ena._collect_objc_bindings(
            '@implementation SCView\n+ (void)bindAttributes:(id)b {\n'
            '  [b bindAttribute:@"value" invalidateLayoutOnChange:YES withTextBlock:nil resetBlock:nil];\n'
            '  // [b bindAttribute:@"commented" ...];\n}\n@end\n'
            '@implementation SCPrivate\n- (void)x { [b bindAttribute:@"other" ...]; }\n@end\n',
            col,
        )
        api = ena._finish(col)
        self.assertEqual(api["SCView"]["bindings"], ["value"])
        self.assertNotIn("SCPrivate", api)

    def test_end_inside_string_does_not_truncate_bindings(self):
        col = ena._Collector()
        ena._parse_objc_header("@interface SCView : UIView\n@end\n", "SCView.h", col)
        ena._collect_objc_bindings(
            '@implementation SCView\n+ (void)bindAttributes:(id)b {\n'
            '  NSLog(@"@end of setup");\n'
            '  [b bindAttribute:@"after" invalidateLayoutOnChange:YES withTextBlock:nil resetBlock:nil];\n'
            '}\n@end\n',
            col,
        )
        self.assertEqual(ena._finish(col)["SCView"]["bindings"], ["after"])


class SwiftParserTests(unittest.TestCase):
    def test_public_surface(self):
        api = _parse(
            ena._parse_swift,
            """
public protocol Provider {
    func runtime() async -> Runtime
    var name: String { get }
}
public final class Promise<T>: Base {
    public init(_ value: T) {}
    public func cancel() {}
    func internalThing() {}
    private var state = 0
}
internal class Hidden { public func nope() {} }
public enum Kind {
    case a, b(Int)
}
extension String {
    public func toC() -> CString { fatalError() }
}
""",
            "Test.swift",
        )
        self.assertEqual(sorted(api["Provider"]["members"]), ["name", "runtime"])
        self.assertEqual(api["Provider"]["members"]["name"], ["var name: String { get }"])
        self.assertEqual(sorted(api["Promise"]["members"]), ["cancel", "init"])
        self.assertNotIn("Hidden", api)
        self.assertEqual(sorted(api["Kind"]["members"]), ["a", "b"])
        self.assertIn("toC", api["extension String"]["members"])


class EndToEndTests(unittest.TestCase):
    """Run the real check against a copy of the tree with targeted breakage."""

    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="api_surface_test_"))
        for rel in ena.scanned_roots() + TS_SOURCES:
            matches = sorted(OPEN_SOURCE.glob(rel)) if "*" in rel else [OPEN_SOURCE / rel]
            for src in matches:
                if not src.exists():
                    continue
                dst = self.tmp / src.relative_to(OPEN_SOURCE)
                if src.is_dir():
                    shutil.copytree(src, dst, dirs_exist_ok=True)
                else:
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(src, dst)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_check(self):
        proc = subprocess.run(
            [sys.executable, str(CHECK), str(self.tmp)],
            capture_output=True,
            text=True,
        )
        output = ANSI.sub("", proc.stdout + proc.stderr)
        if os.environ.get("API_SURFACE_TEST_VERBOSE"):
            print(f"\n--- {self.id()} (exit {proc.returncode}) ---\n{output}")
        return proc.returncode, output

    def edit(self, rel, old, new):
        path = self.tmp / rel
        text = path.read_text(encoding="utf-8")
        self.assertIn(old, text, f"fixture drifted: {old!r} not in {rel}")
        path.write_text(text.replace(old, new, 1), encoding="utf-8")

    def test_unmodified_copy_passes(self):
        code, output = self.run_check()
        self.assertEqual(code, 0, output)

    def test_deleting_android_view_class_fails(self):
        (self.tmp / "valdi/src/java/com/snap/valdi/views/ValdiTextView.kt").unlink()
        code, output = self.run_check()
        self.assertEqual(code, 1, output)
        self.assertIn("Removed android class 'com.snap.valdi.views.ValdiTextView'", output)

    def test_deleting_ios_view_class_fails(self):
        (self.tmp / "valdi/src/valdi/ios/Views/SCValdiLabel.h").unlink()
        code, output = self.run_check()
        self.assertEqual(code, 1, output)
        self.assertIn("Removed ios class 'SCValdiLabel'", output)

    def test_dropping_a_binding_fails(self):
        self.edit(
            "valdi/src/java/com/snap/valdi/attributes/impl/AbstractTextViewAttributesBinder.kt",
            'attributesBindingContext.bindTextAttribute("value"',
            '// attributesBindingContext.bindTextAttribute("value"',
        )
        code, output = self.run_check()
        self.assertEqual(code, 1, output)
        self.assertIn("no longer binds attribute 'value'", output)

    def test_changing_a_binder_signature_fails(self):
        self.edit(
            "valdi/src/java/com/snap/valdi/attributes/AttributesBinder.kt",
            "fun bindAttributes(attributesBindingContext: AttributesBindingContext<T>)",
            "fun bindAttributes(attributesBindingContext: AttributesBindingContext<T>, extra: Int)",
        )
        code, output = self.run_check()
        self.assertEqual(code, 1, output)
        self.assertIn("Changed android member 'com.snap.valdi.attributes.AttributesBinder.bindAttributes'", output)

    def test_additions_pass(self):
        (self.tmp / "valdi/src/java/com/snap/valdi/views/ValdiNewView.kt").write_text(
            "package com.snap.valdi.views\n\nclass ValdiNewView(context: Context) : ValdiView(context) {\n"
            "    fun newThing() {}\n}\n",
            encoding="utf-8",
        )
        self.edit(
            "valdi/src/valdi/ios/Views/SCValdiLabel.h",
            "@end",
            "- (void)valdi_newThing;\n@end",
        )
        code, output = self.run_check()
        self.assertEqual(code, 0, output)
        self.assertIn("Added android class 'com.snap.valdi.views.ValdiNewView'", output)
        self.assertIn("Added ios member 'SCValdiLabel.-valdi_newThing'", output)


if __name__ == "__main__":
    unittest.main()
