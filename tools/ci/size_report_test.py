#!/usr/bin/env python3
"""Unit tests for size_report.py. Run: python3 tools/ci/size_report_test.py

size_report.sh runs these before every report, so the delta math cannot rot unnoticed.
"""

import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import size_report as sr  # noqa: E402

KIB = 1024


def report(entries, fmt="quickjs", commit="a" * 40):
    return {"version": 1, "bytecode_format": fmt, "commit": commit, "entries": entries}


def vm(name, size, unc=None):
    e = {"kind": "valdimodule", "target": "//src/valdi_modules/src/valdi/%s:android.release.valdimodule" % name, "bytes": size}
    if unc is not None:
        e["uncompressed_bytes"] = unc
    return e


def native(size, lib="libhello_world_export.so", abi="arm64-v8a"):
    return {"kind": "native", "target": "%s/%s" % (abi, lib), "bytes": size}


class Base(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.dir)

    def write(self, name, data):
        path = os.path.join(self.dir, name)
        with open(path, "w", encoding="utf-8") as f:
            if isinstance(data, str):
                f.write(data)
            else:
                json.dump(data, f)
        return path

    def compare(self, current, baseline=None, message=None, annotate=True, note=None):
        args = ["compare", "--current", self.write("current.json", current)]
        if baseline is not None:
            args += ["--baseline", self.write("baseline.json", baseline)]
        if note:
            args += ["--baseline-note", note]
        if message is not None:
            args += ["--commit-message-file", self.write("msg.txt", message)]
        summary = os.path.join(self.dir, "summary.md")
        if os.path.exists(summary):
            os.remove(summary)
        args += ["--summary", summary]
        if annotate:
            args.append("--annotate")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = sr.main(args)
        self.assertEqual(rc, 0, "compare must always exit 0")
        with open(summary, encoding="utf-8") as f:
            return out.getvalue(), f.read()


class DeltaMath(Base):
    def test_pct_and_new(self):
        self.assertEqual(sr.pct(50, 1000), 5.0)
        self.assertEqual(sr.pct(10, 0), float("inf"))
        self.assertEqual(sr.pct(0, 0), 0.0)

    def test_rows_cover_added_and_removed(self):
        cur = sr.validate_report(report([vm("a", 2000), vm("new", 500)]))
        base = sr.validate_report(report([vm("a", 1000), vm("gone", 300)]))
        rows = {r["target"].split(":")[0].rsplit("/", 1)[-1]: r for r in sr.diff_entries(cur, base)}
        self.assertEqual(rows["a"]["delta"], 1000)
        self.assertEqual(rows["a"]["pct"], 100.0)
        self.assertIsNone(rows["new"]["before"])
        self.assertEqual(rows["new"]["delta"], 500)
        self.assertIsNone(rows["gone"]["after"])
        self.assertEqual(rows["gone"]["delta"], -300)

    def test_totals_recomputed_not_trusted(self):
        data = report([vm("a", 100, 400), vm("b", 50, 100), native(7)])
        data["totals"] = {"valdimodule_bytes": 999999}
        t = sr.validate_report(data)["totals"]
        self.assertEqual(t["valdimodule_bytes"], 150)
        self.assertEqual(t["valdimodule_uncompressed_bytes"], 500)
        self.assertEqual(t["native_bytes"], {"arm64-v8a": 7})

    def test_uncompressed_total_omitted_when_partial(self):
        t = sr.compute_totals(sr.validate_report(report([vm("a", 100, 400), vm("b", 50)]))["entries"])
        self.assertNotIn("valdimodule_uncompressed_bytes", t)


class Thresholds(Base):
    def crossings(self, cur_entries, base_entries):
        cur = sr.validate_report(report(cur_entries))
        base = sr.validate_report(report(base_entries))
        return sr.find_crossings(cur, base, sr.diff_entries(cur, base))

    # A big filler module keeps the TOTAL threshold quiet so these isolate the per-module rule.
    FILLER = vm("filler", 100 * 1024 * KIB)

    def test_module_needs_both_absolute_and_relative(self):
        # +10 KiB but only +1%: no warning (AND, not OR).
        self.assertEqual(self.crossings([vm("a", 1010 * KIB), self.FILLER], [vm("a", 1000 * KIB), self.FILLER]), [])
        # +50% but only +5 KiB: no warning.
        self.assertEqual(self.crossings([vm("a", 15 * KIB), self.FILLER], [vm("a", 10 * KIB), self.FILLER]), [])
        # +10 KiB and +10%: warns.
        w = self.crossings([vm("a", 110 * KIB), self.FILLER], [vm("a", 100 * KIB), self.FILLER])
        self.assertEqual(len(w), 1)
        self.assertIn("valdi/a:android.release.valdimodule grew", w[0])

    def test_new_module_over_10k_warns(self):
        w = self.crossings([vm("a", 12 * KIB), self.FILLER], [self.FILLER])
        self.assertEqual(len(w), 1)
        self.assertIn("(new)", w[0])

    def test_shrink_never_warns(self):
        self.assertEqual(self.crossings([vm("a", 1 * KIB)], [vm("a", 900 * KIB)]), [])

    def test_native_either_absolute_or_relative(self):
        base = 10 * 1024 * KIB  # 10 MiB
        # +20 KiB (0.2%): warns on absolute.
        self.assertEqual(len(self.crossings([native(base + 20 * KIB)], [native(base)])), 1)
        # +3% of a small lib, under 20 KiB: warns on relative.
        self.assertEqual(len(self.crossings([native(103 * KIB)], [native(100 * KIB)])), 1)
        # +19 KiB and +0.2%: quiet.
        self.assertEqual(self.crossings([native(base + 19 * KIB)], [native(base)]), [])

    def test_native_is_per_abi(self):
        w = self.crossings(
            [native(100 * KIB, abi="arm64-v8a"), native(200 * KIB, abi="armeabi-v7a")],
            [native(100 * KIB, abi="arm64-v8a"), native(100 * KIB, abi="armeabi-v7a")],
        )
        self.assertEqual(len(w), 1)
        self.assertIn("armeabi-v7a", w[0])

    def test_total_either_absolute_or_relative(self):
        # Ten modules, each +6 KiB on 1 MiB: no module crosses (needs 10 KiB), the total does (+60 KiB).
        base = [vm("m%d" % i, 1024 * KIB) for i in range(10)]
        cur = [vm("m%d" % i, 1030 * KIB) for i in range(10)]
        w = self.crossings(cur, base)
        self.assertEqual(len(w), 1)
        self.assertIn("total .valdimodule size grew", w[0])
        # +2% of a small total, under 50 KiB: warns on relative.
        w = self.crossings([vm("a", 102 * KIB)], [vm("a", 100 * KIB)])
        self.assertEqual([x for x in w if x.startswith("total")], [w[-1]])


class Rendering(Base):
    def test_no_baseline_is_loud_and_absolute(self):
        out, md = self.compare(report([vm("a", 5000, 20000), native(9000)]), note="No baseline report was found.")
        self.assertIn("No baseline", md)
        self.assertIn("NOT a comparison", md)
        self.assertIn("NO BASELINE", out)
        self.assertNotIn("::warning", out)
        self.assertIn("4.9 KiB", md)  # absolute size listed

    def test_table_lists_only_1k_changes_top_20(self):
        filler = vm("zz_filler", 100 * 1024 * KIB)
        base = [vm("m%02d" % i, 100 * KIB) for i in range(30)] + [vm("tiny", 100 * KIB), filler]
        cur = [vm("m%02d" % i, 100 * KIB + (i + 2) * KIB) for i in range(30)] + [vm("tiny", 100 * KIB + 500), filler]
        _, md = self.compare(report(cur), report(base))
        rows = [line for line in md.splitlines() if line.startswith("| valdimodule")]
        self.assertEqual(len(rows), 20)
        self.assertIn("m29", rows[0])  # largest change first
        self.assertNotIn("tiny", md)
        self.assertIn("10 more change(s)", md)

    def test_annotation_emitted_when_enabled(self):
        out, md = self.compare(report([vm("a", 200 * KIB)]), report([vm("a", 100 * KIB)]))
        self.assertIn("::warning title=Valdi size::", out)
        self.assertIn("threshold crossing", md)

    def test_report_only_by_default(self):
        out, md = self.compare(report([vm("a", 200 * KIB)]), report([vm("a", 100 * KIB)]), annotate=False)
        self.assertNotIn("::warning", out)
        self.assertIn("report-only", md)

    def test_trailer_suppresses_annotations_keeps_table(self):
        msg = "Add a big thing\n\nSize-change: new codec tables | `rm -rf` <b>\nSigned-off-by: x\n"
        out, md = self.compare(report([vm("a", 200 * KIB)]), report([vm("a", 100 * KIB)]), message=msg)
        self.assertNotIn("::warning", out)
        self.assertIn("acknowledged by a `Size-change:` trailer", md)
        self.assertIn("| valdimodule |", md)
        self.assertIn("new codec tables", md)
        self.assertNotIn("| `rm", md)  # reason is escaped, cannot break the table
        self.assertIn("\\|", md)

    def test_trailer_detection(self):
        self.assertEqual(sr.find_trailer("x\n\nsize-change: ok\n"), "ok")
        self.assertEqual(sr.find_trailer("x\nSize-change:\n"), "(no reason given)")
        self.assertIsNone(sr.find_trailer("mentions Size-change: mid-line only"))
        self.assertIsNone(sr.find_trailer(""))
        self.assertEqual(len(sr.find_trailer("Size-change: " + "y" * 1000)), sr.MAX_REASON_CHARS)

    def test_annotation_escaping(self):
        self.assertEqual(sr.gh_escape("a%b\nc\r"), "a%25b%0Ac%0D")


class MalformedBaseline(Base):
    CUR = report([vm("a", 200 * KIB)])

    def assert_rejected(self, baseline, why=None):
        path = self.write("baseline.json", baseline)
        with self.assertRaises(sr.ReportError):
            sr.load_report(path)
        out, md = self.compare(self.CUR, baseline)
        self.assertIn("No baseline", md)
        self.assertIn("rejected", md)
        self.assertNotIn("::warning", out)
        if why:
            self.assertIn(why, md)

    def test_not_json(self):
        self.assert_rejected("{not json", "not valid JSON")

    def test_wrong_shapes(self):
        self.assert_rejected([1, 2, 3])
        self.assert_rejected(dict(self.CUR, version=2))
        self.assert_rejected(dict(self.CUR, entries="nope"))
        self.assert_rejected(report([{"kind": "valdimodule", "target": "//a:b", "bytes": "12"}]))
        self.assert_rejected(report([{"kind": "valdimodule", "target": "//a:b", "bytes": True}]))
        self.assert_rejected(report([{"kind": "valdimodule", "target": "//a:b", "bytes": -1}]))
        self.assert_rejected(report([{"kind": "other", "target": "//a:b", "bytes": 1}]))
        self.assert_rejected(report([vm("a", 1), vm("a", 2)]))
        self.assert_rejected(report([native(1, abi="")]))

    def test_injection_in_names_rejected(self):
        self.assert_rejected(report([{"kind": "valdimodule", "target": "//a:b\n::error::pwned", "bytes": 1}]))
        self.assert_rejected(report([{"kind": "valdimodule", "target": "<img src=x>", "bytes": 1}]))
        self.assert_rejected(report([{"kind": "valdimodule", "target": "a|b", "bytes": 1}]))
        self.assert_rejected(report([], fmt="quick js"))
        self.assert_rejected(report([], commit="$(id)"))

    def test_oversized(self):
        self.assert_rejected(" " * (sr.MAX_REPORT_BYTES + 1), "larger than")

    def test_too_many_entries(self):
        self.assert_rejected(report([vm("m%d" % i, 1) for i in range(sr.MAX_ENTRIES + 1)]))

    def test_bytecode_mismatch_is_no_baseline(self):
        out, md = self.compare(self.CUR, report([vm("a", 100 * KIB)], fmt="hermes"))
        self.assertIn("different JS bytecode format", md)
        self.assertNotIn("::warning", out)

    def test_native_measure_mismatch_skips_native_only(self):
        cur = dict(report([vm("a", 200 * KIB), native(900 * KIB)]), native_measure="stripped")
        base = dict(report([vm("a", 100 * KIB), native(100 * KIB)]), native_measure="packaged")
        out, md = self.compare(cur, base)
        self.assertIn("Native libraries not compared", md)
        self.assertNotIn("native libraries (arm64-v8a) grew", out)
        self.assertIn("valdi/a:android.release.valdimodule grew", out)

    def test_native_missing_on_one_side_skips_native(self):
        out, md = self.compare(report([vm("a", 100 * KIB)]), report([vm("a", 100 * KIB), native(100 * KIB)]))
        self.assertIn("this run has no native measurements", md)
        self.assertNotIn("| native |", md)
        out, md = self.compare(report([vm("a", 100 * KIB), native(100 * KIB)]), report([vm("a", 100 * KIB)]))
        self.assertIn("the baseline has no native measurements", md)

    def test_invalid_native_measure(self):
        self.assert_rejected(dict(report([]), native_measure="whatever"))

    def test_broken_current_still_exits_0(self):
        out, _ = self.compare_current_broken()
        self.assertIn("could not be read", out)

    def compare_current_broken(self):
        path = self.write("current.json", "garbage")
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            self.assertEqual(sr.main(["compare", "--current", path]), 0)
        return out.getvalue(), None


class Collect(Base):
    def test_collect_ignores_and_sorts(self):
        files = {}
        for name, size in (("valdi_core", 300), ("jasmine", 999), ("coreutils", 100)):
            files[name] = self.write(name + ".valdimodule", "x" * size)
        so = self.write("lib.so", "y" * 77)
        tsv = self.write(
            "vm.tsv",
            "".join("//src/valdi_modules/src/valdi/%s:android.release.valdimodule\t%s\n" % (n, p) for n, p in files.items()),
        )
        ntsv = self.write("n.tsv", "arm64-v8a/libhello_world_export.so\t%s\n" % so)
        ignore = self.write("ignore.txt", "# comment\njasmine   # test only\n\n")
        out = os.path.join(self.dir, "r.json")
        with contextlib.redirect_stdout(io.StringIO()):
            sr.main(["collect", "--valdimodules", tsv, "--native", ntsv, "--ignore", ignore,
                     "--bytecode-format", "quickjs", "--commit", "b" * 40, "--out", out])
        r = sr.load_report(out)
        targets = [e["target"] for e in r["entries"]]
        # Sorted by (kind, target): native before valdimodule.
        self.assertEqual(targets[0], "arm64-v8a/libhello_world_export.so")
        self.assertFalse(any("jasmine" in t for t in targets))
        self.assertEqual(targets[1:], sorted(targets[1:]))
        self.assertEqual(len(targets), 3)
        self.assertEqual(r["totals"]["valdimodule_bytes"], 400)
        self.assertEqual(r["totals"]["native_bytes"], {"arm64-v8a": 77})

    def test_module_name(self):
        self.assertEqual(sr.module_name("//src/valdi_modules/src/valdi/jasmine:android.release.valdimodule"), "jasmine")
        self.assertEqual(sr.module_name("//modules/hello_world:android.release.valdimodule"), "hello_world")


if __name__ == "__main__":
    unittest.main()
