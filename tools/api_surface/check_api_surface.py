#!/usr/bin/env python3
"""Compare the current Valdi API surface against a baseline.

Warns on additions (new interfaces, properties, types).
Errors on incompatible changes (removals, type changes).

Exit codes:
  0 — no changes
  1 — breaking changes detected
  2 — additions only (use --strict to make this an error)

Usage:
  check_api_surface.py <open_source_root>            # check against baseline
  check_api_surface.py <open_source_root> --update    # regenerate baseline
  check_api_surface.py <open_source_root> --strict    # treat additions as errors
"""

import json
import sys
from pathlib import Path

from extract_api import extract_api

BASELINE_PATH = Path(__file__).parent / "baseline.json"

YELLOW = "\033[33m"
RED = "\033[31m"
GREEN = "\033[32m"
BOLD = "\033[1m"
RESET = "\033[0m"


def warn(msg: str):
    print(f"  {YELLOW}WARNING{RESET}: {msg}")


def error(msg: str):
    print(f"  {RED}ERROR{RESET}:   {msg}")


def _diff_dict(baseline: dict, current: dict, kind: str):
    """Compare two flat dicts. Returns (warnings, errors)."""
    warnings = []
    errors = []

    baseline_keys = set(baseline.keys())
    current_keys = set(current.keys())

    for key in sorted(baseline_keys - current_keys):
        errors.append(f"Removed {kind} '{key}'")

    for key in sorted(current_keys - baseline_keys):
        warnings.append(f"Added {kind} '{key}': {current[key]}")

    for key in sorted(baseline_keys & current_keys):
        if baseline[key] != current[key]:
            errors.append(
                f"Changed {kind} '{key}':\n"
                f"         baseline: {baseline[key]}\n"
                f"         current:  {current[key]}"
            )

    return warnings, errors


def _diff_interfaces(baseline: dict, current: dict):
    """Compare interface definitions. Returns (warnings, errors)."""
    warnings = []
    errors = []

    baseline_names = set(baseline.keys())
    current_names = set(current.keys())

    for name in sorted(baseline_names - current_names):
        errors.append(f"Removed interface '{name}'")

    for name in sorted(current_names - baseline_names):
        prop_count = len(current[name].get("properties", {}))
        warnings.append(f"Added interface '{name}' ({prop_count} properties)")

    for name in sorted(baseline_names & current_names):
        b_iface = baseline[name]
        c_iface = current[name]

        # Check extends chain changes
        b_ext = b_iface.get("extends", [])
        c_ext = c_iface.get("extends", [])
        removed_ext = set(b_ext) - set(c_ext)
        added_ext = set(c_ext) - set(b_ext)
        for ext in sorted(removed_ext):
            errors.append(f"'{name}' no longer extends '{ext}'")
        for ext in sorted(added_ext):
            warnings.append(f"'{name}' now extends '{ext}'")

        # Check properties
        b_props = b_iface.get("properties", {})
        c_props = c_iface.get("properties", {})

        b_keys = set(b_props.keys())
        c_keys = set(c_props.keys())

        for prop in sorted(b_keys - c_keys):
            errors.append(f"Removed '{name}.{prop}' (was: {b_props[prop]})")

        for prop in sorted(c_keys - b_keys):
            warnings.append(f"Added '{name}.{prop}': {c_props[prop]}")

        for prop in sorted(b_keys & c_keys):
            if b_props[prop] != c_props[prop]:
                errors.append(
                    f"Changed '{name}.{prop}':\n"
                    f"         baseline: {b_props[prop]}\n"
                    f"         current:  {c_props[prop]}"
                )

    return warnings, errors


def _diff_classes(baseline: dict, current: dict):
    """Compare class definitions. Returns (warnings, errors)."""
    warnings = []
    errors = []

    baseline_names = set(baseline.keys())
    current_names = set(current.keys())

    for name in sorted(baseline_names - current_names):
        errors.append(f"Removed class '{name}'")

    for name in sorted(current_names - baseline_names):
        warnings.append(f"Added class '{name}'")

    for name in sorted(baseline_names & current_names):
        b_cls = baseline[name]
        c_cls = current[name]

        # Check extends
        if b_cls.get("extends") != c_cls.get("extends"):
            errors.append(
                f"Changed '{name}' extends:\n"
                f"         baseline: {b_cls.get('extends')}\n"
                f"         current:  {c_cls.get('extends')}"
            )

        # Check methods
        b_methods = b_cls.get("methods", {})
        c_methods = c_cls.get("methods", {})

        b_keys = set(b_methods.keys())
        c_keys = set(c_methods.keys())

        for method in sorted(b_keys - c_keys):
            errors.append(f"Removed '{name}.{method}' (was: {b_methods[method]})")

        for method in sorted(c_keys - b_keys):
            warnings.append(f"Added '{name}.{method}': {c_methods[method]}")

        for method in sorted(b_keys & c_keys):
            if b_methods[method] != c_methods[method]:
                errors.append(
                    f"Changed '{name}.{method}':\n"
                    f"         baseline: {b_methods[method]}\n"
                    f"         current:  {c_methods[method]}"
                )

    return warnings, errors


def check(open_source_root: Path, strict: bool = False) -> int:
    """Run the API surface check. Returns exit code."""
    if not BASELINE_PATH.exists():
        print(f"{RED}No baseline found at {BASELINE_PATH}{RESET}")
        print("Run with --update to generate one.")
        return 1

    baseline = json.loads(BASELINE_PATH.read_text())
    current = extract_api(open_source_root)

    all_warnings = []
    all_errors = []

    # Interfaces
    print(f"\n{BOLD}Checking interfaces...{RESET}")
    w, e = _diff_interfaces(
        baseline.get("interfaces", {}), current.get("interfaces", {})
    )
    all_warnings.extend(w)
    all_errors.extend(e)

    # Type aliases
    print(f"{BOLD}Checking type aliases...{RESET}")
    w, e = _diff_dict(baseline.get("types", {}), current.get("types", {}), "type")
    all_warnings.extend(w)
    all_errors.extend(e)

    # Enums
    print(f"{BOLD}Checking enums...{RESET}")
    b_enums_flat = {}
    c_enums_flat = {}
    for ename, members in baseline.get("enums", {}).items():
        for mname, mval in members.items():
            b_enums_flat[f"{ename}.{mname}"] = mval
    for ename, members in current.get("enums", {}).items():
        for mname, mval in members.items():
            c_enums_flat[f"{ename}.{mname}"] = mval
    w, e = _diff_dict(b_enums_flat, c_enums_flat, "enum member")
    all_warnings.extend(w)
    all_errors.extend(e)

    # Classes
    print(f"{BOLD}Checking classes...{RESET}")
    w, e = _diff_classes(baseline.get("classes", {}), current.get("classes", {}))
    all_warnings.extend(w)
    all_errors.extend(e)

    # Report
    print()
    if all_errors:
        print(f"{RED}{BOLD}Breaking changes detected:{RESET}")
        for msg in all_errors:
            error(msg)
        print()

    if all_warnings:
        print(f"{YELLOW}{BOLD}API additions detected:{RESET}")
        for msg in all_warnings:
            warn(msg)
        print()

    if not all_errors and not all_warnings:
        print(f"{GREEN}API surface unchanged.{RESET}")
        return 0

    if all_errors:
        total = len(all_errors)
        print(
            f"{RED}{BOLD}{total} breaking change(s) — "
            f"update baseline with --update if intentional.{RESET}"
        )
        return 1

    # Warnings only
    total = len(all_warnings)
    print(
        f"{YELLOW}{BOLD}{total} addition(s) — "
        f"update baseline with --update to accept.{RESET}"
    )
    if strict:
        return 2
    return 0


def update(open_source_root: Path):
    """Regenerate the baseline file."""
    current = extract_api(open_source_root)
    BASELINE_PATH.write_text(json.dumps(current, indent=2, sort_keys=True) + "\n")
    iface_count = len(current.get("interfaces", {}))
    type_count = len(current.get("types", {}))
    enum_count = len(current.get("enums", {}))
    class_count = len(current.get("classes", {}))
    print(
        f"{GREEN}Baseline updated: "
        f"{iface_count} interfaces, {type_count} types, "
        f"{enum_count} enums, {class_count} classes{RESET}"
    )


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    root = Path(sys.argv[1])
    strict = "--strict" in sys.argv
    if "--update" in sys.argv:
        update(root)
    else:
        sys.exit(check(root, strict=strict))


if __name__ == "__main__":
    main()
