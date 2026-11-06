#!/usr/bin/env python3
"""Fail before building if the host's root-only overrides drift from Valdi."""

import ast
from pathlib import Path
import sys


def declarations(path):
    result = {name: {} for name in ("bazel_dep", "single_version_override", "local_path_override")}
    for statement in ast.parse(path.read_text(), filename=str(path)).body:
        if not isinstance(statement, ast.Expr) or not isinstance(statement.value, ast.Call):
            continue
        call = statement.value
        if not isinstance(call.func, ast.Name):
            continue
        kind = call.func.id
        # Fail explicitly if the root starts using a new override/include shape.
        if kind == "include" or (kind.endswith("_override") and kind not in result):
            raise ValueError(f"{path}: teach check_config.py to handle {kind} before building")
        if kind in result:
            fields = {
                item.arg: ast.literal_eval(item.value)
                for item in call.keywords
                if item.arg in ("name", "module_name", "version", "path")
            }
            name = fields.get("module_name", fields.get("name"))
            if name in result[kind]:
                raise ValueError(f"{path}: duplicate {kind} for {name}")
            result[kind][name] = fields
    return result


def check(root, host):
    parent = declarations(root / "MODULE.bazel")
    child = declarations(host / "MODULE.bazel")
    expected_pins = {
        name: fields.get("version", parent["bazel_dep"].get(name, {}).get("version", ""))
        for name, fields in parent["single_version_override"].items()
    }
    actual_pins = {
        name: fields.get("version", "")
        for name, fields in child["single_version_override"].items()
        if name != "sdl3"
    }
    expected_paths = {
        name: (root / fields["path"]).resolve()
        for name, fields in parent["local_path_override"].items()
    }
    actual_paths = {
        name: (host / fields["path"]).resolve()
        for name, fields in child["local_path_override"].items()
        if name != "valdi"
    }
    errors = []
    for name, fields in child["bazel_dep"].items():
        if name in parent["bazel_dep"]:
            expected = parent["bazel_dep"][name].get("version", "")
            if fields.get("version", "") != expected:
                errors.append(f"direct dependency: {name}: expected version {expected!r}")
    for label, expected, actual in (
        ("version overrides", expected_pins, actual_pins),
        ("source overrides", expected_paths, actual_paths),
    ):
        for name in sorted(expected.keys() | actual.keys()):
            if expected.get(name) != actual.get(name):
                errors.append(f"{label}: {name}: expected {expected.get(name)!r}, got {actual.get(name)!r}")
    if (root / ".bazelversion").read_text().strip() != (host / ".bazelversion").read_text().strip():
        errors.append(".bazelversion differs from the parent checkout")
    return errors


if __name__ == "__main__":
    host = Path(__file__).resolve().parent
    try:
        errors = check(host.parent, host)
    except (ValueError, SyntaxError) as error:
        errors = [str(error)]
    if errors:
        sys.exit("Linux host configuration drift; update it with the parent checkout:\n" + "\n".join(errors))
    print("Linux host version/source overrides and Bazel version match the parent checkout.")
