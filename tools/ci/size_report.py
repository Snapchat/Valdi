#!/usr/bin/env python3
"""Valdi module size report: measure, compare against a baseline, render. Warn-only.

Driven by tools/ci/size_report.sh; see that script for the CI flow. Standard library only.

Subcommands:
  collect  Measure .valdimodule files and native libraries into a size-report.json.
  compare  Diff a current report against an (optional, untrusted) baseline report and write a
           Markdown summary plus, when enabled, GitHub Actions ::warning annotations.

size-report.json schema (version 1):
  {
    "version": 1,
    "bytecode_format": "quickjs",
    "commit": "<sha or 'unknown'>",
    "native_measure": "stripped",      # or "packaged" (as found in the AAR, no strip tool found)
    "entries": [
      {"kind": "valdimodule", "target": "//pkg:android.release.valdimodule",
       "bytes": 123, "uncompressed_bytes": 456},
      {"kind": "native", "target": "arm64-v8a/libfoo.so", "bytes": 789}
    ],
    "totals": {"valdimodule_bytes": 123, "valdimodule_uncompressed_bytes": 456,
               "native_bytes": {"arm64-v8a": 789}}
  }

A baseline report comes from an artifact uploaded by an earlier run, so it is treated as untrusted
input: it is size-capped, schema-validated, and only numbers and pattern-checked target names from
it are ever rendered. A baseline that fails validation is reported as "no baseline", never trusted
partially.

The process always exits 0 on `compare`; it is a report, not a gate.
"""

import argparse
import json
import math
import os
import re
import subprocess
import sys

SCHEMA_VERSION = 1
KIB = 1024

# Thresholds. Only growth warns; shrinkage is listed in the table but never annotated.
MODULE_MIN_DELTA = 10 * KIB  # per .valdimodule, compressed: +10 KiB AND +5%
MODULE_MIN_PCT = 5.0
NATIVE_MIN_DELTA = 20 * KIB  # per native library ABI: +20 KiB OR +3%
NATIVE_MIN_PCT = 3.0
TOTAL_MIN_DELTA = 50 * KIB  # total compressed .valdimodule bytes: +50 KiB OR +2%
TOTAL_MIN_PCT = 2.0
TABLE_MIN_DELTA = 1 * KIB  # rows smaller than this are not listed
TABLE_MAX_ROWS = 20

# Hard limits applied to any report read from disk.
MAX_REPORT_BYTES = 1024 * 1024
MAX_ENTRIES = 2000
MAX_SIZE_VALUE = 1 << 40
TARGET_RE = re.compile(r"^[A-Za-z0-9_@/:.+=,-]{1,200}$")
ABI_RE = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
FORMAT_RE = re.compile(r"^[a-z0-9_]{1,32}$")
COMMIT_RE = re.compile(r"^([0-9a-f]{7,64}|unknown)$")
KINDS = ("valdimodule", "native")
NATIVE_MEASURES = ("stripped", "packaged")

TRAILER_RE = re.compile(r"^Size-change:[ \t]*(.*)$", re.IGNORECASE | re.MULTILINE)
MAX_REASON_CHARS = 200


class ReportError(Exception):
    pass


# --------------------------------------------------------------------------------------------
# Validation
# --------------------------------------------------------------------------------------------


def _is_size(value):
    return isinstance(value, int) and not isinstance(value, bool) and 0 <= value < MAX_SIZE_VALUE


def validate_report(data):
    """Returns a normalized copy of `data`, or raises ReportError. Unknown keys are dropped."""
    if not isinstance(data, dict):
        raise ReportError("report is not a JSON object")
    if data.get("version") != SCHEMA_VERSION:
        raise ReportError("unsupported report version")
    fmt = data.get("bytecode_format")
    if not isinstance(fmt, str) or not FORMAT_RE.match(fmt):
        raise ReportError("invalid bytecode_format")
    commit = data.get("commit")
    if not isinstance(commit, str) or not COMMIT_RE.match(commit):
        raise ReportError("invalid commit")
    measure = data.get("native_measure", "packaged")
    if measure not in NATIVE_MEASURES:
        raise ReportError("invalid native_measure")
    entries = data.get("entries")
    if not isinstance(entries, list) or len(entries) > MAX_ENTRIES:
        raise ReportError("invalid entries")

    out_entries = []
    seen = set()
    for e in entries:
        if not isinstance(e, dict):
            raise ReportError("entry is not an object")
        kind = e.get("kind")
        target = e.get("target")
        if kind not in KINDS:
            raise ReportError("invalid entry kind")
        if not isinstance(target, str) or not TARGET_RE.match(target):
            raise ReportError("invalid entry target")
        if (kind, target) in seen:
            raise ReportError("duplicate entry")
        seen.add((kind, target))
        if not _is_size(e.get("bytes")):
            raise ReportError("invalid entry bytes")
        norm = {"kind": kind, "target": target, "bytes": e["bytes"]}
        if "uncompressed_bytes" in e and e["uncompressed_bytes"] is not None:
            if not _is_size(e["uncompressed_bytes"]):
                raise ReportError("invalid entry uncompressed_bytes")
            norm["uncompressed_bytes"] = e["uncompressed_bytes"]
        if kind == "native" and not ABI_RE.match(native_abi(target)):
            raise ReportError("native target must be '<abi>/<lib>'")
        out_entries.append(norm)

    return {
        "version": SCHEMA_VERSION,
        "bytecode_format": fmt,
        "commit": commit,
        "native_measure": measure,
        "entries": out_entries,
        # Totals are always recomputed from entries, never read, so a baseline cannot claim totals
        # that disagree with its own entries.
        "totals": compute_totals(out_entries),
    }


def load_report(path):
    try:
        size = os.path.getsize(path)
    except OSError as exc:
        raise ReportError("cannot read report: %s" % exc.strerror)
    if size > MAX_REPORT_BYTES:
        raise ReportError("report is larger than %d bytes" % MAX_REPORT_BYTES)
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, UnicodeDecodeError, ValueError):
        raise ReportError("report is not valid JSON")
    return validate_report(data)


# --------------------------------------------------------------------------------------------
# Measurement
# --------------------------------------------------------------------------------------------


def native_abi(target):
    return target.split("/", 1)[0] if "/" in target else ""


def compute_totals(entries):
    vm = [e for e in entries if e["kind"] == "valdimodule"]
    totals = {"valdimodule_bytes": sum(e["bytes"] for e in vm), "native_bytes": {}}
    if vm and all("uncompressed_bytes" in e for e in vm):
        totals["valdimodule_uncompressed_bytes"] = sum(e["uncompressed_bytes"] for e in vm)
    for e in entries:
        if e["kind"] == "native":
            abi = native_abi(e["target"])
            totals["native_bytes"][abi] = totals["native_bytes"].get(abi, 0) + e["bytes"]
    return totals


def load_ignore(path):
    """Ignore-list entries: a full label, or a bare module name matching the label's package."""
    patterns = set()
    if not path or not os.path.exists(path):
        return patterns
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.split("#", 1)[0].strip()
            if line:
                patterns.add(line)
    return patterns


def module_name(label):
    """//src/valdi_modules/src/valdi/jasmine:android.release.valdimodule -> jasmine"""
    pkg = label.split(":", 1)[0]
    return pkg.rstrip("/").rsplit("/", 1)[-1]


def is_ignored(label, patterns):
    return label in patterns or module_name(label) in patterns


def zstd_uncompressed_size(path):
    """Decompressed size via the zstd CLI, or None when it is unavailable or the file is not zstd."""
    try:
        proc = subprocess.Popen(["zstd", "-dcq", path], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    except OSError:
        return None
    total = 0
    while True:
        chunk = proc.stdout.read(1 << 20)
        if not chunk:
            break
        total += len(chunk)
    proc.stdout.close()
    if proc.wait() != 0:
        return None
    return total


def cmd_collect(args):
    patterns = load_ignore(args.ignore)
    entries = []
    if args.valdimodules:
        with open(args.valdimodules, "r", encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line:
                    continue
                label, _, path = line.partition("\t")
                if not path or is_ignored(label, patterns):
                    continue
                entry = {"kind": "valdimodule", "target": label, "bytes": os.path.getsize(path)}
                unc = zstd_uncompressed_size(path)
                if unc is not None:
                    entry["uncompressed_bytes"] = unc
                entries.append(entry)
    if args.native:
        with open(args.native, "r", encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line:
                    continue
                target, _, path = line.partition("\t")
                if path:
                    entries.append({"kind": "native", "target": target, "bytes": os.path.getsize(path)})

    entries.sort(key=lambda e: (e["kind"], e["target"]))
    report = validate_report(
        {
            "version": SCHEMA_VERSION,
            "bytecode_format": args.bytecode_format,
            "commit": args.commit or "unknown",
            "native_measure": args.native_measure,
            "entries": entries,
        }
    )
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2, sort_keys=True)
        f.write("\n")
    print("Wrote %s: %d entries" % (args.out, len(entries)))
    return 0


# --------------------------------------------------------------------------------------------
# Comparison
# --------------------------------------------------------------------------------------------


def pct(delta, base):
    if base == 0:
        return math.inf if delta > 0 else 0.0
    return 100.0 * delta / base


def diff_entries(current, baseline):
    """Rows for every (kind, target) on either side, including added and removed ones."""
    cur = {(e["kind"], e["target"]): e["bytes"] for e in current["entries"]}
    base = {(e["kind"], e["target"]): e["bytes"] for e in baseline["entries"]}
    rows = []
    for key in sorted(set(cur) | set(base)):
        before = base.get(key)
        after = cur.get(key)
        b = before or 0
        a = after or 0
        rows.append(
            {
                "kind": key[0],
                "target": key[1],
                "before": before,
                "after": after,
                "delta": a - b,
                "pct": pct(a - b, b),
            }
        )
    return rows


def crosses_module(row):
    return row["delta"] >= MODULE_MIN_DELTA and row["pct"] >= MODULE_MIN_PCT


def crosses_native(delta, base):
    return delta > 0 and (delta >= NATIVE_MIN_DELTA or pct(delta, base) >= NATIVE_MIN_PCT)


def crosses_total(delta, base):
    return delta > 0 and (delta >= TOTAL_MIN_DELTA or pct(delta, base) >= TOTAL_MIN_PCT)


def find_crossings(current, baseline, rows):
    """List of human-readable warning strings (no markup)."""
    warnings = []
    for r in rows:
        if r["kind"] == "valdimodule" and crosses_module(r):
            warnings.append(
                "%s grew by %s (%s), now %s"
                % (r["target"], fmt_delta(r["delta"]), fmt_pct(r["pct"]), fmt_bytes(r["after"]))
            )
    cur_native = current["totals"]["native_bytes"]
    base_native = baseline["totals"]["native_bytes"]
    for abi in sorted(set(cur_native) | set(base_native)):
        a = cur_native.get(abi, 0)
        b = base_native.get(abi, 0)
        if b and crosses_native(a - b, b):
            warnings.append(
                "native libraries (%s) grew by %s (%s), now %s"
                % (abi, fmt_delta(a - b), fmt_pct(pct(a - b, b)), fmt_bytes(a))
            )
    a = current["totals"]["valdimodule_bytes"]
    b = baseline["totals"]["valdimodule_bytes"]
    if crosses_total(a - b, b):
        warnings.append(
            "total .valdimodule size grew by %s (%s), now %s"
            % (fmt_delta(a - b), fmt_pct(pct(a - b, b)), fmt_bytes(a))
        )
    return warnings


# --------------------------------------------------------------------------------------------
# Rendering
# --------------------------------------------------------------------------------------------


def fmt_bytes(n):
    if n is None:
        return "-"
    if abs(n) >= KIB * KIB:
        return "%.2f MiB" % (n / (KIB * KIB))
    if abs(n) >= KIB:
        return "%.1f KiB" % (n / KIB)
    return "%d B" % n


def fmt_delta(n):
    return ("+" if n > 0 else "") + fmt_bytes(n) if n else "0 B"


def fmt_pct(p):
    if p == math.inf:
        return "new"
    return "%+.1f%%" % p


def md_cell(text):
    """Escapes a string for a Markdown table cell. Inputs are already pattern-checked; this is
    defense in depth so a pipe or backtick can never break out of the cell."""
    text = str(text)[:200]
    return re.sub(r"([\\`|*_\[\]<>#!~&])", r"\\\1", text)


def gh_escape(text):
    """Escapes a ::warning message per the workflow-command spec."""
    return str(text).replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def sanitize_reason(text):
    text = re.sub(r"[^\x20-\x7e]", "", text).strip()
    return text[:MAX_REASON_CHARS]


def find_trailer(message):
    """Returns the sanitized reason from a `Size-change: <reason>` trailer, or None."""
    if not message:
        return None
    m = TRAILER_RE.search(message)
    if not m:
        return None
    return sanitize_reason(m.group(1)) or "(no reason given)"


def render_absolute(current, lines):
    lines.append("| Kind | Target | Size | Uncompressed |")
    lines.append("|---|---|---:|---:|")
    for e in sorted(current["entries"], key=lambda e: -e["bytes"])[:TABLE_MAX_ROWS]:
        lines.append(
            "| %s | %s | %s | %s |"
            % (e["kind"], md_cell(e["target"]), fmt_bytes(e["bytes"]), fmt_bytes(e.get("uncompressed_bytes")))
        )


def render_totals(current, baseline, lines):
    lines.append("")
    lines.append("| Total | Baseline | Current | Change |")
    lines.append("|---|---:|---:|---:|")

    def row(label, b, a):
        if baseline is None or b is None:
            lines.append("| %s | - | %s | - |" % (label, fmt_bytes(a)))
        else:
            lines.append("| %s | %s | %s | %s (%s) |" % (label, fmt_bytes(b), fmt_bytes(a), fmt_delta(a - b), fmt_pct(pct(a - b, b))))

    ct = current["totals"]
    bt = baseline["totals"] if baseline else {}
    row(".valdimodule (compressed)", bt.get("valdimodule_bytes"), ct["valdimodule_bytes"])
    if "valdimodule_uncompressed_bytes" in ct:
        row(".valdimodule (uncompressed)", bt.get("valdimodule_uncompressed_bytes"), ct["valdimodule_uncompressed_bytes"])
    for abi in sorted(ct["native_bytes"]):
        row("native %s" % md_cell(abi), bt.get("native_bytes", {}).get(abi), ct["native_bytes"][abi])


def build_summary(current, baseline, baseline_note, warnings, rows, annotate, reason):
    lines = ["## Valdi size report", ""]
    lines.append(
        "JS bytecode format: `%s`. Native libraries: %s. Android release outputs. Warn-only: this never fails the build."
        % (current["bytecode_format"], current["native_measure"])
    )
    lines.append("")
    if baseline is None:
        lines.append("> [!WARNING]")
        lines.append("> **No baseline: sizes below are absolute, NOT a comparison.** %s" % md_cell(baseline_note))
        lines.append("> Do not read the absence of size warnings as evidence that nothing grew.")
        lines.append("")
        render_absolute(current, lines)
        render_totals(current, None, lines)
        return "\n".join(lines) + "\n"

    lines.append("Baseline: `%s`." % baseline["commit"][:12])
    lines.append("")
    if warnings:
        if reason is not None:
            lines.append("**%d threshold crossing(s), acknowledged by a `Size-change:` trailer:** %s" % (len(warnings), md_cell(reason)))
        elif not annotate:
            lines.append("**%d threshold crossing(s)** (report-only rollout: no annotations emitted):" % len(warnings))
        else:
            lines.append("**%d threshold crossing(s):**" % len(warnings))
        lines.append("")
        for w in warnings:
            lines.append("- %s" % md_cell(w))
        lines.append("")
    else:
        lines.append("No threshold crossed.")
        lines.append("")

    listed = [r for r in rows if abs(r["delta"]) >= TABLE_MIN_DELTA]
    listed.sort(key=lambda r: (-abs(r["delta"]), r["kind"], r["target"]))
    if listed:
        lines.append("| Kind | Target | Baseline | Current | Change |")
        lines.append("|---|---|---:|---:|---:|")
        for r in listed[:TABLE_MAX_ROWS]:
            lines.append(
                "| %s | %s | %s | %s | %s (%s) |"
                % (r["kind"], md_cell(r["target"]), fmt_bytes(r["before"]), fmt_bytes(r["after"]), fmt_delta(r["delta"]), fmt_pct(r["pct"]))
            )
        if len(listed) > TABLE_MAX_ROWS:
            lines.append("")
            lines.append("_%d more change(s) of at least 1 KiB not shown._" % (len(listed) - TABLE_MAX_ROWS))
    else:
        lines.append("No per-target change of 1 KiB or more.")
    render_totals(current, baseline, lines)
    return "\n".join(lines) + "\n"


def has_native(report):
    return any(e["kind"] == "native" for e in report["entries"])


def without_native(report):
    entries = [e for e in report["entries"] if e["kind"] != "native"]
    return dict(report, entries=entries, totals=compute_totals(entries))


def cmd_compare(args):
    try:
        current = load_report(args.current)
    except ReportError as exc:
        # Our own measurement is broken. Say so; still a report, not a gate.
        print("::warning title=Valdi size::size report could not be read: %s" % gh_escape(exc))
        return 0

    baseline = None
    note = args.baseline_note or ""
    if args.baseline:
        try:
            baseline = load_report(args.baseline)
        except ReportError as exc:
            note = "The baseline artifact was rejected (%s)." % exc
            baseline = None
    if baseline is not None and baseline["bytecode_format"] != current["bytecode_format"]:
        note = "The baseline used a different JS bytecode format, so sizes are not comparable."
        baseline = None
    if baseline is None and not note:
        note = "No baseline report was found."
    native_note = ""
    if baseline is not None:
        cur_has = has_native(current)
        base_has = has_native(baseline)
        if cur_has != base_has:
            # One side failed to measure; a one-sided list would read as every library being
            # added or removed.
            native_note = "Native libraries not compared: %s has no native measurements." % (
                "this run" if base_has else "the baseline")
        elif baseline["native_measure"] != current["native_measure"]:
            # Stripped vs as-packaged sizes differ by the symbol tables; comparing them is meaningless.
            native_note = "Native libraries not compared: baseline is %s, this run is %s." % (
                baseline["native_measure"], current["native_measure"])
        if native_note:
            baseline = without_native(baseline)
            current = without_native(current)

    message = ""
    if args.commit_message_file and os.path.exists(args.commit_message_file):
        with open(args.commit_message_file, "r", encoding="utf-8", errors="replace") as f:
            message = f.read(64 * KIB)
    reason = find_trailer(message)

    rows = diff_entries(current, baseline) if baseline else []
    warnings = find_crossings(current, baseline, rows) if baseline else []
    summary = build_summary(current, baseline, note, warnings, rows, args.annotate, reason)
    if native_note:
        summary += "\n%s\n" % native_note

    if args.summary:
        with open(args.summary, "a", encoding="utf-8") as f:
            f.write(summary)
    else:
        sys.stdout.write(summary)

    if baseline is None:
        print("NO BASELINE: Valdi size report shows absolute sizes only. %s" % note)
    for w in warnings:
        if args.annotate and reason is None:
            print("::warning title=Valdi size::%s" % gh_escape(w))
        else:
            print("size threshold crossed (not annotated): %s" % w)
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    c = sub.add_parser("collect")
    c.add_argument("--valdimodules", help="TSV file: <label>\\t<path> per line")
    c.add_argument("--native", help="TSV file: <abi>/<lib>\\t<path> per line")
    c.add_argument("--ignore", help="ignore list (labels or module names)")
    c.add_argument("--bytecode-format", required=True)
    c.add_argument("--commit", default="unknown")
    c.add_argument("--native-measure", choices=NATIVE_MEASURES, default="packaged",
                   help="whether the native sizes are of stripped copies or as packaged")
    c.add_argument("--out", required=True)

    d = sub.add_parser("compare")
    d.add_argument("--current", required=True)
    d.add_argument("--baseline", help="baseline size-report.json (untrusted)")
    d.add_argument("--baseline-note", help="why there is no baseline, when there is none")
    d.add_argument("--commit-message-file", help="PR head commit message, checked for a Size-change: trailer")
    d.add_argument("--summary", help="append Markdown here (default: stdout)")
    d.add_argument("--annotate", action="store_true", help="emit ::warning annotations")

    args = p.parse_args(argv)
    if args.cmd == "collect":
        return cmd_collect(args)
    return cmd_compare(args)


if __name__ == "__main__":
    sys.exit(main())
